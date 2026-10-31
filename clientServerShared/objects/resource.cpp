#include <global_root.h>
#include "resource.h"
#include "../grid/resources.h"
#include <string/encoding.h>

namespace Objects
{
	namespace Resource
	{
		namespace TagMap
		{
			String		ToString(char32_t tag)
			{
				if (tag == 0)
					return "";
				StringEncoding::UTF8::TChar c;
				StringEncoding::UTF32::ToUTF8(tag,c);
				String rs (TStringLength(c.numCharsUsed));
				memcpy(rs.mutablePointer(),c.encoded,c.numCharsUsed);
				return rs;
			}

			char		IDToTag(ID idx)
			{
				return IndexToTag(idx.serialIndex);
			}

			char		IndexToTag(index_t idx)
			{
				if (idx >= Count)
					throw Except::Program::ParameterFault(CLOCATION,"Given parameter index "+String(idx)+" exceeds limit of "+String(Count));
				if (idx < 5)
					return char(3 + idx);
				idx -= 5;
				if (idx < 2)
					return char(11+idx);
				idx -= 2;
				return char(14+idx);
			}

			index_t		TagToIndex(char32_t c)
			{
				if (c < '\t') //9
					return index_t(c)-3;
				if (c < '\r')
					return index_t(index_t(c)-'\n')+4;
				return index_t(index_t(c) - '\r') + 6;
			}

			ID		TagToID(char32_t c)
			{
				return TagToIndex(c);
			}

			bool	IsTag(char32_t c)
			{
				if (c < '\t')
					return c > 0;
				if (c < '\r')
					return c > '\n';
				return c > '\r' && c < ' ';
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

		const String&	ID::GetIcon() const
		{
			const auto&t = table[serialIndex];
			if (t.icon.IsNotEmpty())
				return t.icon;
			return t.name;
		}

		void		Type::OnDeserialize(index_t serialIndex)
		{
			this->serialIndex = serialIndex;
			tag = serialIndex < TagMap::Count ? TagMap::IndexToTag(serialIndex) : 0;
		}


		String		Type::GetTagStr() const
		{
			if (tag != 0)
				return String(StringType::CastExpression<char>(StringRef(&tag,1)));
			return '('+name+") ";
		}



		const Table minimal =
		Table(
		{
			"Fuel",
			"Minerals",
			"Uranium",
			"Energy",
			#ifndef NO_MUNITION
				"Munitions,
			#endif
		},
		{
			float3(0.4,1,0.2),//fuel
			float3(0.7,0.7,0.7),//minerals
			float3(1,1,0.1), //uranium
			float3(0.2,0.4,1),//energy
			#ifndef NO_MUNITION
				float3(1,0.4,0.2),//munitions
			#endif
		}	
		);

		const ID	
			Fuel = FuelID, 
			Minerals = MineralID,
			Uranium = UraniumID,
			Energy = 3,
			#ifndef NO_MUNITION
				Munitions = 4,
				Offset = 5,
			#else
				Munitions = ID::Invalid,
				Offset = 4,
			#endif
			CrystallineCompounds = Offset,
			OrganicCompounds = Offset+1,
			CompositeMaterials = Offset+2,
			_ = ID::Invalid;

		Table lastTable, table = CompileLocalTable();

		Table::Table(const std::initializer_list<const char*>&init, const std::initializer_list<float3>&colors)
		{
			for (index_t i = 0; i < init.size(); i++)
			{
				Append(*(init.begin() + i), *(colors.begin() + i));
			}
		}

		ID		Table::Append(TypePrototype&&t)
		{
			return Append(Type(std::move(t)));
		}

		ID		Table::Append(const TypePrototype&t)
		{
			return Append(Type(t));
		}

		ID		Table::Append(Type&&t)
		{
			const index_t rs = Count();
			Super::MoveAppend(t).OnDeserialize(rs);
			return rs;
		}

		
		ID		Table::Append(const String&name, const float3&color)
		{
			return Append(name.c_str(),"",color);
		}
		
		ID		Table::Append(String&&name, const float3&color)
		{
			return Append(Type(std::move(name),color));
		}
		
		ID		Table::Append(const char*name, const float3&color)
		{
			return Append(Type(String(name),color));
		}

		ID		Table::Append(const String&name, const String&icon, const float3&color)
		{
			return Append(name.c_str(),icon.c_str(),color);
		}

		ID		Table::Append(String&&name, String&&icon, const float3&color)
		{
			return Append(Type(std::move(name),std::move(icon),color));
		}

		ID		Table::Append(const char*name, const char*icon, const float3&color)
		{
			return Append(Type(String(name),String(icon),color));
		}

		void	Table::Set(ID at, const char*name, const char*icon, const float3&color)
		{
			while (Count() <= at.serialIndex)
				Super::Append();
			Type&t = Super::at(at.serialIndex);
			if (t.name.IsNotEmpty() && t.name != name)
				FATAL__("Trying to set resource table entry #"+String(at.serialIndex)+" to '"+String(name)+"' but is already set to '"+t.name+"'");
			t.name = name;
			t.icon = icon;
			t.color = color*255;
			t.OnDeserialize(at.serialIndex);
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


		int&	Vector::Require(index_t idx)
		{
			if (idx < storage.Count())
				return storage[idx];
			count_t old = storage.Count();
			storage.ResizePreserveContent(idx+1);
			for (index_t i = old; i <= idx; i++)
				storage[i] = 0;
			return storage[idx];
		}


		void	Vector::Set(index_t resource, int value)
		{
			if (resource >= storage.Count() && value == 0)
				return;
			int &r = Require(resource);
			r = value;
			if (r == 0)
				Tidy();
		}
	
		void	Vector::Add(index_t resource, int value)
		{
			if (value == 0)
				return;
			int &r = Require(resource);
			r += value;
			if (r == 0)
				Tidy();
		}


		void	Vector::Tidy()
		{
			if (storage.IsEmpty())
				return;
			index_t i = storage.Count()-1;
			for (; i < storage.Count(); i--)
				if (storage[i] != 0)
					break;
			storage.ResizePreserveContent(i+1);
		}


		void	Vector::operator=(const CoreVector&v)
		{
			if (v.IsEmpty())
			{
				Clear();
				return;
			}
			Set(Resource::Fuel,v.fuel);
			Set(Resource::Minerals,v.minerals);
			Set(Resource::Uranium,v.uranium);
		}


		void	Vector::operator+=(const CoreVector&v)
		{
			if (v.IsEmpty())
				return;
			if (IsZero())
			{
				operator=(v);
				return;
			}
			Add(Resource::Fuel,v.fuel);
			Add(Resource::Minerals,v.minerals);
			Add(Resource::Uranium,v.uranium);
		}


		void	Vector::operator+=(const Self&other)
		{
			if (other.IsZero())
				return;
			if (IsZero())
			{
				storage = other.storage;
				return;
			}
			for (index_t i = 0; i < other.storage.Count(); i++)
				Add(i,other.storage[i]);
		}

		void	Vector::operator-=(const Self&other)
		{
			if (other.IsZero())
				return;
			if (IsZero())
			{
				storage = other.storage;
				Negate();
				return;
			}
			for (index_t i = 0; i < other.storage.Count(); i++)
				Add(i, -other.storage[i]);
		}

		void	Vector::operator*=(int multiplier)
		{
			if (multiplier == 0)
				storage.Clear();
			else
			{
				foreach (storage, v)
					(*v) *= multiplier;
			}
		}

		bool	Vector::operator==(const Self&other) const
		{
			return storage == other.storage;
		}

		bool	Vector::operator!=(const Self&other) const
		{
			return storage != other.storage;
		}


		Vector	Vector::operator+(const Self&v) const
		{
			Vector rs = *this;
			rs += v;
			return rs;
		}


		Vector	Vector::operator-(const Self&other)	const
		{
			Vector rs = *this;
			rs -= other;
			return rs;
		}

		int		Vector::operator/(const Self&v) const
		{
			int rs = std::numeric_limits<int>::max();
			if (IsZero())
				return 0;
			if (v.IsZero())
				return rs;

			for (index_t i = 0; i < v.storage.Count(); i++)
			{
				auto other = v.storage[i];
				auto local = Get(i);
				if (other != 0)
				{
					if (std::abs(local) < std::abs(other))
						return 0;
					int div = local / other;
					if (std::abs(div) < std::abs(rs))
						rs = div;
				}
			}
			return rs;
		}


		Vector	Vector::operator*(int multiplier) const
		{
			Vector rs = *this;
			rs *= multiplier;
			return rs;
		}

		int		Vector::Sum() const
		{
			int rs = 0;
			foreach (storage,v)
				rs += (*v);
			return rs;
		}

		int		Vector::SumNegative() const
		{
			int rs = 0;
			foreach (storage,v)
				if (*v < 0)
					rs += (*v);
			return rs;
		}

		int		Vector::SumPositive() const
		{
			int rs = 0;
			foreach (storage,v)
				if (*v > 0)
					rs += (*v);
			return rs;
		}

		void	Vector::SeparateBySign(Self&positive, Self&negative) const
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

		void	Vector::Cap(const Self&limit)
		{
			for (index_t i = 0; i < storage.Length(); i++)
				storage[i] = std::min(storage[i],limit[i]);
		}

		void	Vector::Negate()
		{
			foreach (storage, v)
				*v = -*v;
		}

		int		Vector::operator[](index_t resource) const
		{
			return resource < storage.Length() ? storage.at(resource) : 0;
		}


		String	Vector::ToString() const
		{
			Ctr::Vector<String> rs;
			for (index_t i = 0; i < Resource::table.Count(); i++)
				if (Get(i) != 0)
					rs << Resource::table[i].name+": "+String(Get(i));
			return implode(',',rs.pointer(),rs.Count());
		}

		bool	Vector::IsZero() const
		{
			return storage.IsEmpty();
		}

		bool	Vector::IsNonZero() const
		{
			return storage.IsNotEmpty();
		}

		bool	Vector::AnyIsNonZeroPositive() const
		{
			foreach (storage,v)
				if (*v > 0)
					return true;
			return false;
		}


		bool	Vector::IsStable() const
		{
			foreach (storage,v)
				if (*v < 0)
					return false;
			return true;
		}

		bool	Vector::AnyIsSet(const FlagSet&set) const
		{
			for (index_t i = 0; i < storage.Length(); i++)
				if (storage[i] && (set & i))
					return true;
			return false;
		}

		FlagSet	Vector::IdentifySet() const
		{
			FlagSet rs;
			for (index_t i = 0; i < storage.Length(); i++)
				if (storage[i])
					rs.Set(i);
			return rs;
		}


		void	FullVector::Include(const Vector&v, const Vector&storage, const Vector&storageMax)
		{
			Vector s,d;
			v.SeparateBySign(s,d);
			d.Negate();
			supply += s;
			demand += d;
			balance += v;
			balance += storage;
			this->storage += storage;
			this->storageMax += storageMax;
		}

		bool	CoreVector::SampleWorld(index_t lowCellID, bool translateToActual/*=true*/)
		{
			#ifdef CLIENT
				if (!Grid::ResourceDiscovery::client.resourcesDiscovered[lowCellID])
				{
					*this = CoreVector();
					return false;
				}
			#endif
			fuel = Grid::Resource::GetFuel().coverage[lowCellID];
			minerals = Grid::Resource::GetMinerals().coverage[lowCellID];
			uranium = Grid::Resource::GetUranium().coverage[lowCellID];

			if (translateToActual)
				this->operator*=(TranslationFactor);

			return true;
		}






		/**/	PositiveVector::PositiveVector(const Super&v)
		{
			((Super&)*this) = v;
			DBG_ASSERT__(IsStable());
		}

		



		/*static*/	void					PositiveVector::Serialize(IWriteStream&outStream, const Super&vec)
		{
			auto set = vec.IdentifySet();
			outStream.WriteSize(set.GetNumeric());
			for (index_t i = 0; i < Resource::table.Count(); i++)
				if (vec[i] > 0)
					outStream.WriteSize((serial_size_t)(vec[i]));
		}
		
		/*static*/	void					PositiveVector::Deserialize(IReadStream&inStream, Super&vec)
		{
			FlagSet::flag_t numeric;
			inStream.ReadSize(numeric);
			auto flags = FlagSet::FromNumeric(numeric);
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


		count_t FlagSet::Count() const
		{
			auto i = isSet;
			//https://stackoverflow.com/questions/2709430/count-number-of-bits-in-a-64-bit-long-big-integer
			i = i - ((i >> 1) & 0x5555555555555555);
			i = (i & 0x3333333333333333) + ((i >> 2) & 0x3333333333333333);
			return (((i + (i >> 4)) & 0xF0F0F0F0F0F0F0F) * 0x101010101010101) >> 56;
		}


		Table CompileLocalTable()
		{
			Table rs = minimal;
			
			

			rs.Set(CrystallineCompounds, "Crystalline Compounds", "", float3(0.4,0.3,0.7));
			rs.Set(OrganicCompounds, "Organic Compounds", "",float3(0.6,1,0.3));
			rs.Set(CompositeMaterials, "Composite Materials", "", float3(0.4,0.4,0.4));
			return rs;
		}


	}


	String GetTagName(char32_t c)
	{
		if (!IsTag(c))
			return StringEncoding::UTF32::ToUTF8(c).ToRef();
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
