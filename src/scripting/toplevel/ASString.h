/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2013  Alessandro Pignotti (a.pignotti@sssup.it)

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

#ifndef SCRIPTING_TOPLEVEL_ASSTRING_H
#define SCRIPTING_TOPLEVEL_ASSTRING_H 1

#include "scripting/class.h"

namespace Glib { class ustring; }

namespace lightspark
{
/*
 * The AS String class.
 * The 'data' is immutable -> it cannot be changed after creation of the object
 */
class ASString: public ASObject
{
	friend ASString* abstract_s(SystemState* sys);
	friend ASString* abstract_s(SystemState* sys, const char* s, uint32_t len);
	friend ASString* abstract_s(SystemState* sys, const char* s);
	friend ASString* abstract_s(SystemState* sys, const tiny_string& s);
	friend ASString* abstract_s(SystemState* sys, uint32_t stringId);
private:
	number_t parseStringInfinite(const char *s, char **end) const;
	tiny_string data;
	_NR<ASObject> strlength;
public:
	ASString(Class_base* c);
	ASString(Class_base* c, const std::string& s);
	ASString(Class_base* c, const tiny_string& s);
	ASString(Class_base* c, const Glib::ustring& s);
	ASString(Class_base* c, const char* s);
	ASString(Class_base* c, const char* s, uint32_t len);
	bool hasId:1;
	bool datafilled:1;
	inline tiny_string& getData()
	{
		if (!datafilled)
		{
			data = getSystemState()->getStringFromUniqueId(stringId);
			datafilled = true;
		}
		return data;
	}
	inline bool isEmpty() const
	{
		if (hasId)
			return stringId == BUILTIN_STRINGS::EMPTY || stringId == UINT32_MAX;
		return data.empty();
	}

	static void sinit(Class_base* c);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(charAt);
	ASFUNCTION_ATOM(charCodeAt);
	ASFUNCTION(concat);
	ASFUNCTION(fromCharCode);
	ASFUNCTION(indexOf);
	ASFUNCTION(lastIndexOf);
	ASFUNCTION(match);
	ASFUNCTION(replace);
	ASFUNCTION(search);
	ASFUNCTION(slice);
	ASFUNCTION(split);
	ASFUNCTION(substr);
	ASFUNCTION(substring);
	ASFUNCTION(toLowerCase);
	ASFUNCTION(toUpperCase);
	ASFUNCTION(_toString);
	ASFUNCTION_ATOM(_getLength);
	ASFUNCTION(localeCompare);
	ASFUNCTION(localeCompare_prototype);
	bool isEqual(ASObject* r);
	TRISTATE isLess(ASObject* r);
	number_t toNumber();
	int32_t toInt();
	uint32_t toUInt();
	int64_t toInt64();
	
	ASFUNCTION(generator);
	//Serialization interface
	void serialize(ByteArray* out, std::map<tiny_string, uint32_t>& stringMap,
				std::map<const ASObject*, uint32_t>& objMap,
				std::map<const Class_base*, uint32_t>& traitsMap);
	std::string toDebugString() { return std::string("\"") + std::string(getData()) + "\""; }
	static bool isEcmaSpace(uint32_t c);
	static bool isEcmaLineTerminator(uint32_t c);
	inline bool destruct() 
	{ 
		data.clear(); 
		strlength.reset();
		hasId = false;
		datafilled=false; 
		if (!ASObject::destruct())
		{
			stringId = BUILTIN_STRINGS::EMPTY;
			hasId = true;
			datafilled = true;
			return false;
		}
		return true;
	}
};

template<>
inline asAtom Class<ASString>::coerce(SystemState* sys,asAtom o) const
{
	if (o.type == T_STRING)
		return o;
	//Special handling for Null and Undefined follows avm2overview's description of 'coerce_s' opcode
	if(o.type == T_NULL)
		return o;
	asAtom res;
	if(o.type == T_UNDEFINED)
	{
		res.type = T_NULL;
		ASATOM_DECREF(o);
		return res;
	}
	if(!o.isConstructed())
		return o;
	res = asAtom::fromObject(lightspark::abstract_s(sys,o.toString()));
	ASATOM_DECREF(o);
	return res;
}

}
#endif /* SCRIPTING_TOPLEVEL_ASSTRING_H */
