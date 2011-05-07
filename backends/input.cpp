/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2011  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "scripting/abc.h"
#include "input.h"
#include "swf.h"
#include "rendering.h"
#include "compat.h"

#include <SDL.h>

#ifdef COMPILE_PLUGIN
#include <gdk/gdkkeysyms.h>
#endif

using namespace lightspark;
using namespace std;

InputThread::InputThread(SystemState* s):m_sys(s),terminated(false),threaded(false),
	mutexListeners("Input listeners"),mutexDragged("Input dragged"),curDragged(NULL),lastMouseDownTarget(NULL),
	mutexInputData("Input data")
{
	LOG(LOG_NO_INFO,_("Creating input thread"));
}

void InputThread::start(ENGINE e,void* param)
{
	if(e==SDL)
	{
		threaded=true;
		pthread_create(&t,NULL,(thread_worker)sdl_worker,this);
	}
#ifdef COMPILE_PLUGIN
	else if(e==GTKPLUG)
	{
		npapi_params=(NPAPI_params*)param;
		GtkWidget* container=npapi_params->container;
		gtk_widget_set_can_focus(container,True);
		gtk_widget_add_events(container,GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
						GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_EXPOSURE_MASK | GDK_VISIBILITY_NOTIFY_MASK |
						GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK);
		g_signal_connect(G_OBJECT(container), "event", G_CALLBACK(gtkplug_worker), this);
	}
#endif
	else
		::abort();
}

InputThread::~InputThread()
{
	wait();
}

void InputThread::wait()
{
	if(terminated)
		return;
	if(threaded)
		pthread_join(t,NULL);
	terminated=true;
}

#ifdef COMPILE_PLUGIN
//This is a GTK event handler and the gdk lock is already acquired
gboolean InputThread::gtkplug_worker(GtkWidget *widget, GdkEvent *event, InputThread* th)
{
	//Set sys to this SystemState
	sys=th->m_sys;
	gboolean ret=FALSE;
	switch(event->type)
	{
		case GDK_KEY_PRESS:
		{
			//cout << "key press" << endl;
			switch(event->key.keyval)
			{
				case GDK_p:
					th->m_sys->showProfilingData=!th->m_sys->showProfilingData;
					break;
				case GDK_m:
					if (!th->m_sys->audioManager->pluginLoaded())
						break;
					th->m_sys->audioManager->toggleMuteAll();
					if(th->m_sys->audioManager->allMuted())
						LOG(LOG_NO_INFO, "All sounds muted");
					else
						LOG(LOG_NO_INFO, "All sounds unmuted");
					break;
				case GDK_c:
					if(th->m_sys->hasError())
					{
						GtkClipboard *clipboard;
						clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
						gtk_clipboard_set_text(clipboard, th->m_sys->getErrorCause().c_str(),
								th->m_sys->getErrorCause().size());
						clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
						gtk_clipboard_set_text(clipboard, th->m_sys->getErrorCause().c_str(),
								th->m_sys->getErrorCause().size());
						LOG(LOG_NO_INFO, "Copied error to clipboard");
					}
					else
						LOG(LOG_NO_INFO, "No error to be coppied to clipboard");
					break;
				default:
					break;
			}
			ret=TRUE;
			break;
		}
		case GDK_EXPOSE:
		{
			//Signal the renderThread
			th->m_sys->getRenderThread()->draw(false);
			ret=TRUE;
			break;
		}
		case GDK_BUTTON_PRESS:
		{
			//Grab focus, to receive keypresses
			gtk_widget_grab_focus(widget);
			th->handleMouseDown(event->button.x,event->button.y);
			ret=TRUE;
			break;
		}
		case GDK_BUTTON_RELEASE:
		{
			th->handleMouseUp(event->button.x,event->button.y);
			ret=TRUE;
			break;
		}
		default:
//#ifdef EXPENSIVE_DEBUG
//			cout << "GDKTYPE " << event->type << endl;
//#endif
			break;
	}
	return ret;
}
#endif

void InputThread::handleMouseDown(uint32_t x, uint32_t y)
{
	Locker locker(mutexListeners);
	_NR<InteractiveObject>selected = NullRef;
	try
	{
		selected=m_sys->hitTest(NullRef,x,y);
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Error in input handling ") << e.cause);
		m_sys->setError(e.cause);
		return;
	}
	assert(maskStack.empty());
	if(selected==NULL)
		return;
	assert_and_throw(selected->getPrototype()->isSubClass(Class<InteractiveObject>::getClass()));
	lastMouseDownTarget=selected;
	//Add event to the event queue
	m_sys->currentVm->addEvent(selected,_MR(Class<MouseEvent>::getInstanceS("mouseDown",true)));
}

void InputThread::handleMouseUp(uint32_t x, uint32_t y)
{
	Locker locker(mutexListeners);
	_NR<InteractiveObject> selected = NullRef;
	try
	{
		selected=m_sys->hitTest(NullRef,x,y);
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Error in input handling ") << e.cause);
		m_sys->setError(e.cause);
		return;
	}
	assert(maskStack.empty());
	if(selected==NULL)
		return;
	assert_and_throw(selected->getPrototype()->isSubClass(Class<InteractiveObject>::getClass()));
	//Add event to the event queue
	m_sys->currentVm->addEvent(selected,_MR(Class<MouseEvent>::getInstanceS("mouseUp",true)));
	if(lastMouseDownTarget==selected)
	{
		//Also send the click event
		m_sys->currentVm->addEvent(selected,_MR(Class<MouseEvent>::getInstanceS("click",true)));
		lastMouseDownTarget=NullRef;
	}
}

void InputThread::handleMouseMove(uint32_t x, uint32_t y)
{
	Locker locker(mutexInputData);
	mouseX = x;
	mouseY = y;
}

void* InputThread::sdl_worker(InputThread* th)
{
	sys=th->m_sys;
	SDL_Event event;
	while(SDL_WaitEvent(&event))
	{
		if(sys->isShuttingDown())
			break;
		switch(event.type)
		{
			case SDL_KEYDOWN:
			{
				switch(event.key.keysym.sym)
				{
					case SDLK_p:
						th->m_sys->showProfilingData=!th->m_sys->showProfilingData;
						break;
					case SDLK_m:
						if (!th->m_sys->audioManager->pluginLoaded())
							break;
						th->m_sys->audioManager->toggleMuteAll();
						if(th->m_sys->audioManager->allMuted())
							LOG(LOG_NO_INFO, "All sounds muted");
						else
							LOG(LOG_NO_INFO, "All sounds unmuted");
						break;
					case SDLK_q:
						th->m_sys->setShutdownFlag();
						if(th->m_sys->currentVm)
							LOG(LOG_CALLS,_("We still miss ") << sys->currentVm->getEventQueueSize() << _(" events"));
						pthread_exit(0);
						break;
					case SDLK_s:
						th->m_sys->state.stop_FP=true;
						break;
					//Ignore any other keystrokes
					default:
						break;
				}
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
				th->handleMouseDown(event.button.x,event.button.y);
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				th->handleMouseUp(event.button.x,event.button.y);
				break;
			}
			case SDL_MOUSEMOTION:
			{
				th->handleMouseMove(event.motion.x,event.motion.y);
				break;
			}
			case SDL_VIDEORESIZE:
			{
				th->m_sys->getRenderThread()->requestResize(event.resize.w, event.resize.h);
				break;
			}
			case SDL_QUIT:
			{
				th->m_sys->setShutdownFlag();
				if(th->m_sys->currentVm)
					LOG(LOG_CALLS,_("We still miss ") << sys->currentVm->getEventQueueSize() << _(" events"));
				pthread_exit(0);
				break;
			}
		}
	}
	return NULL;
}

void InputThread::addListener(InteractiveObject* ob)
{
	Locker locker(mutexListeners);
	assert(ob);

#ifndef NDEBUG
	vector<InteractiveObject*>::const_iterator it=find(listeners.begin(),listeners.end(),ob);
	//Object is already register, should not happen
	assert_and_throw(it==listeners.end());
#endif
	
	//Register the listener
	listeners.push_back(ob);
}

void InputThread::removeListener(InteractiveObject* ob)
{
	Locker locker(mutexListeners);

	vector<InteractiveObject*>::iterator it=find(listeners.begin(),listeners.end(),ob);
	if(it==listeners.end()) //Listener not found
		return;
	
	//Unregister the listener
	listeners.erase(it);
}

void InputThread::enableDrag(Sprite* s, const lightspark::RECT& limit)
{
	Locker locker(mutexDragged);
	if(s==curDragged)
		return;
	
	if(curDragged) //Stop dragging the previous sprite
		curDragged->decRef();
	
	assert(s);
	//We need to avoid that the object is destroyed
	s->incRef();
	
	curDragged=s;
	dragLimit=limit;
}

void InputThread::disableDrag()
{
	Locker locker(mutexDragged);
	if(curDragged)
	{
		curDragged->decRef();
		curDragged=NULL;
	}
}

bool InputThread::isMasked(number_t x, number_t y) const
{
	for(uint32_t i=0;i<maskStack.size();i++)
	{
		number_t localX, localY;
		maskStack[i].m.multiply2D(x,y,localX,localY);
		if(!maskStack[i].d->isOpaque(localX, localY))
			return false;
	}

	return true;
}
