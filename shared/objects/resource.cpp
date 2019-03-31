#include <global_root.h>
#include "resource.h"
#include "../grid/resources.h"


namespace Objects
{
	namespace Resource
	{
		namespace TagMap
		{
			String		ToString(char tag)
			{
				return tag != 0 ? String(tag) : "";
			}

			char		IDToTag(ID idx)
			{
				return IndexToTag(idx.serialIndex);
			}

			char		IndexToTag(index_t idx)
			{
				if (idx >= Count)
					throw Except::Program::ParameterFault(CLOCATION,"Given parameter index "+String(idx)+" exceeds limit of "+String(Count));
				if (idx < 6)
					return char(3 + idx);
				idx -= 6;
				if (idx < 2)
					return char(11+idx);
				idx -= 2;
				return char(13+idx);
			}

			index_t		TagToIndex(char c)
			{
				if (c < '\n')
					return index_t(c)-3;
				if (c < '\r')
					return index_t(c-'\n')+6;
				return index_t(c - '\r') + 8;
			}

			ID		TagToID(char c)
			{
				return TagToIndex(c);
			}
		}

		/*static*/ const ID	ID::Invalid = ID(InvalidIndex);

		const Type&	ID::ToType(const Table&t) const
		{
			return t[serialIndex];
		}

		const Type&	ID::ToType() const
		{
			return table[serialIndex];
		}

		char		ID::GetTag(const Table&t) const
		{
			return t[serialIndex].tag;
		}

		char		ID::GetTag() const
		{
			return table[serialIndex].tag;
		}

		String		ID::GetTagStr(const Table&t) const
		{
			return t[serialIndex].GetTagStr();
		}

		String		ID::GetTagStr() const
		{
			return table[serialIndex].GetTagStr();
		}

		const String&	ID::ToString() const
		{
			return table[serialIndex].name;
		}


		String		Type::GetTagStr() const
		{
			if (tag != 0)
				return String(tag);
			return '('+name+')';
		}



		const Table standard =
		{
			"Fuel",
			"Minerals",
			"Uranium",
			"Energy",
			#ifndef NO_MUNITION
				"Munitions,
			#endif
		};
		Table table = standard;

		const ID	
			Fuel = 0, 
			Minerals = 1,
			Uranium = 2,
			Energy = 3,
			#ifndef NO_MUNITION
				Munitions = 4,
			#else
				Munitions = ID::Invalid,
			#endif
			_ = ID::Invalid;


		Table::Table(const std::initializer_list<const char*>&init)
		{
			foreach(init, name)
				Append(*name);
		}

		ID		Table::Append(Type&&t, bool updateTag)
		{
			const index_t rs = Count();
			Super::MoveAppend(std::move(t));
			Super::Last().serialIndex = rs;
			if (updateTag)
				Super::Last().tag = rs < TagMap::Count ? TagMap::IndexToTag(rs) : 0;
			return rs;
		}

		ID		Table::Append(const Type&t, bool updateTag)
		{
			const index_t rs = Count();
			Super::Append(t);
			Super::Last().serialIndex = rs;
			if (updateTag)
				Super::Last().tag = rs < TagMap::Count ? TagMap::IndexToTag(rs) : 0;
			return rs;
		}
		
		ID		Table::Append(const String&name)
		{
			return Append(name.c_str());
		}
		
		ID		Table::Append(String&&name)
		{
			const index_t rs = Count();
			Type&t = Super::Append();
			t.name = std::move(name);
			t.serialIndex = rs;
			t.tag = rs < TagMap::Count ? TagMap::IndexToTag(rs) : 0;
			return rs;
		}
		
		ID		Table::Append(const char*name)
		{
			const index_t rs = Count();
			Type&t = Super::Append();
			t.name = name;
			t.serialIndex = rs;
			t.tag = rs < TagMap::Count ? TagMap::IndexToTag(rs) : 0;
			return rs;
		}

		const Type*		Table::FindByName(const String&n) const
		{
			foreach (*this,t)
				if (t->name == n)
					return t;
			return nullptr;
		}

		bool			Table::operator==(const Table&other) const
		{
			if (GetSize() != other.GetSize())
				return false;
			for (index_t i = 0; i < GetSize(); i++)
				if (at(i).name != other[i].name)
					return false;
			return true;
		}


	}

	void	ResourceVector::AllocateEmpty()
	{
		storage.SetSize(Resource::table.Count());
		storage.Fill(0);
	}


	void	ResourceVector::Set(index_t resource, int value)
	{
		if (IsZero() && value == 0)
			return;
		if (storage.IsEmpty())
		{
			AllocateEmpty();
			storage[resource] = value;
		}
		else
		{
			storage[resource] = value;
			if (value == 0)
				CheckEmptiness();
		}
	}
	
	void	ResourceVector::Add(index_t resource, int value)
	{
		if (value == 0)
			return;
		if (storage.IsEmpty())
		{
			AllocateEmpty();
			storage[resource] = value;
		}
		else
		{
			storage[resource] += value;
			if (storage[resource] == 0)
				CheckEmptiness();
		}
	}


	void	ResourceVector::CheckEmptiness()
	{
		foreach (storage,v)
			if ((*v) != 0)
				return;
		storage.Clear();
	}


	void	ResourceVector::operator=(const NaturalResourceVector&v)
	{
		if (v.IsEmpty())
		{
			Clear();
			return;
		}
		AllocateEmpty();
		Set(Resource::Fuel,v.fuel);
		Set(Resource::Minerals,v.minerals);
		Set(Resource::Uranium,v.uranium);
	}

	void	ResourceVector::operator+=(const NaturalResourceVector&v)
	{
		if (v.IsEmpty())
			return;
		if (IsZero())
		{
			operator=(v);
			return;
		}
		storage[Resource::Fuel.serialIndex] += v.fuel;
		storage[Resource::Minerals.serialIndex] += v.minerals;
		storage[Resource::Uranium.serialIndex] += v.uranium;
		CheckEmptiness();
	}


	void	ResourceVector::operator+=(const Self&other)
	{
		if (other.IsZero())
			return;
		if (IsZero())
		{
			storage = other.storage;
			return;
		}
		for (index_t i = 0; i < storage.Count(); i++)
			storage[i] += other.storage[i];
		CheckEmptiness();
	}

	void	ResourceVector::operator-=(const Self&other)
	{
		if (other.IsZero())
			return;
		if (IsZero())
		{
			storage = other.storage;
			Negate();
			return;
		}
		for (index_t i = 0; i < storage.Count(); i++)
			storage[i] -= other.storage[i];
		CheckEmptiness();
	}

	void	ResourceVector::operator*=(int multiplier)
	{
		if (multiplier == 0)
			storage.Clear();
		else
		{
			foreach (storage, v)
				(*v) *= multiplier;
		}
	}

	bool	ResourceVector::operator==(const Self&other) const
	{
		return storage == other.storage;
	}

	bool	ResourceVector::operator!=(const Self&other) const
	{
		return storage != other.storage;
	}

	ResourceVector	ResourceVector::operator-(const Self&other)	const
	{
		ResourceVector rs = *this;
		rs -= other;
		return rs;
	}

	ResourceVector	ResourceVector::operator*(int multiplier) const
	{
		ResourceVector rs = *this;
		rs *= multiplier;
		return rs;
	}

	int		ResourceVector::Sum() const
	{
		int rs = 0;
		foreach (storage,v)
			rs += (*v);
		return rs;
	}

	int		ResourceVector::SumNegative() const
	{
		int rs = 0;
		foreach (storage,v)
			if (*v < 0)
				rs += (*v);
		return rs;
	}

	int		ResourceVector::SumPositive() const
	{
		int rs = 0;
		foreach (storage,v)
			if (*v > 0)
				rs += (*v);
		return rs;
	}

	void	ResourceVector::SeparateBySign(Self&positive, Self&negative) const
	{
		positive.Clear();
		negative.Clear();
		for (index_t i = 0; i < storage.Length(); i++)
		{
			int v = storage[i];
			if (v > 0)
				positive.Set(i,v);
			elif (v < 0)
				negative.Set(i,v);
		}
	}

	void	ResourceVector::Cap(const Self&limit)
	{
		for (index_t i = 0; i < storage.Length(); i++)
			storage[i] = std::min(storage[i],limit[i]);
	}

	void	ResourceVector::Negate()
	{
		foreach (storage, v)
			*v = -*v;
	}

	int		ResourceVector::operator[](index_t resource) const
	{
		return resource < storage.Length() ? storage.at(resource) : 0;
	}


	String	ResourceVector::ToString() const
	{
		Ctr::Buffer0<String> rs;
		for (index_t i = 0; i < Resource::table.Count(); i++)
			if (Get(i) != 0)
				rs << Resource::table[i].name+": "+String(Get(i));
		return implode(',',rs.pointer(),rs.Count());
	}

	bool	ResourceVector::IsZero() const
	{
		return storage.IsEmpty();
	}

	bool	ResourceVector::IsNonZero() const
	{
		return storage.IsNotEmpty();
	}

	bool	ResourceVector::IsStable() const
	{
		foreach (storage,v)
			if (*v < 0)
				return false;
		return true;
	}

	bool	ResourceVector::AnyIsSet(const ResourceFlagSet&set) const
	{
		for (index_t i = 0; i < storage.Length(); i++)
			if (storage[i] && (set & i))
				return true;
		return false;
	}

	ResourceFlagSet	ResourceVector::IdentifySet() const
	{
		ResourceFlagSet rs;
		for (index_t i = 0; i < storage.Length(); i++)
			if (storage[i])
				rs.Set(i);
		return rs;
	}



	bool	NaturalResourceVector::SampleWorld(index_t lowCellID)
	{
		#ifdef CLIENT
			if (!Grid::ResourceDiscovery::client.resourcesDiscovered[lowCellID])
			{
				*this = NaturalResourceVector();
				return false;
			}
		#endif
		fuel = Grid::Resource::GetFuel().coverage[lowCellID] *TranslationFactor;
		minerals = Grid::Resource::GetMinerals().coverage[lowCellID] *TranslationFactor;
		uranium = Grid::Resource::GetUranium().coverage[lowCellID] *TranslationFactor;
		return true;
	}






	/**/	PositiveResourceVector::PositiveResourceVector(const Objects::ResourceVector&v)
	{
		((Objects::ResourceVector&)*this) = v;
		DBG_ASSERT__(IsStable());
	}

		



	/*static*/	void					PositiveResourceVector::Serialize(IWriteStream&outStream, const Objects::ResourceVector&vec)
	{
		auto set = vec.IdentifySet();
		outStream.WriteSize(set.GetNumeric());
		for (index_t i = 0; i < Resource::table.Count(); i++)
			if (vec[i] > 0)
				outStream.WriteSize((serial_size_t)(vec[i]));
	}
		
	/*static*/	void					PositiveResourceVector::Deserialize(IReadStream&inStream, Objects::ResourceVector&vec)
	{
		ResourceFlagSet::flag_t numeric;
		inStream.ReadSize(numeric);
		auto flags = ResourceFlagSet(ResourceFlagSet::TFlagSet(numeric));
		vec.Clear();
		for (index_t i = 0; i < Resource::table.Count(); i++)
		{
			bool exists = flags & i;
			if (!exists)
			{
				continue;
			}
			UINT32 val;
			inStream.ReadSize(val);
			vec.Set(i, val);
		}
	}




	String GetTagName(char c)
	{
		if (!IsTag(c))
			return c;
		index_t idx = Resource::TagMap::TagToIndex(c);
		if (idx != InvalidIndex)
			return Resource::table[idx].name;
		if (c == TimeResourceTag())
			return "time";
		if (c == TrafficResourceTag())
			return "traffic";
		return "";
	}

}
