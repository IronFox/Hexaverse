#define MAKE_MUTLI_SERIALIZABLE_CLASS(SUPER_DEPTH,THIS_DEPTH)\
	template <M_TYPES##THIS_DEPTH>\
		class MultiSerializable##THIS_DEPTH : public MultiSerializable##SUPER_DEPTH <M_T##SUPER_DEPTH>\
		{\
		protected:\
			typedef MultiSerializable##THIS_DEPTH <M_T##THIS_DEPTH>	Self;\
			typedef MultiSerializable##SUPER_DEPTH <M_T##SUPER_DEPTH>	Super;\
			T##SUPER_DEPTH	var##SUPER_DEPTH;\
		public:\
			friend void Deserialize(IReadStream&s, Self&v)\
			{\
				using Serialization::Deserialize;\
				Deserialize(s,(Super&)v);\
				Deserialize(s,v.var##SUPER_DEPTH);\
			}\
			friend void Serialize(IWriteStream&s, const Self&v)\
			{\
				using Serialization::Serialize;\
				Serialize(s,(Super&)v);\
				Serialize(s,v.var##SUPER_DEPTH);\
			}\
			void	Copy(const Self&other)\
			{\
				Super::Copy(other); var##SUPER_DEPTH = other.var##SUPER_DEPTH;\
			}\
			void		swap(Self&other)	{using std::swap; swap(var##SUPER_DEPTH,other.var##SUPER_DEPTH);Super::swap(*this);}\
			friend void swap(Self&a, Self&b) {a.swap(b);}\
		};

				//return	Super::deserialize(inStream) && var##SUPER_DEPTH.deserialize(inStream);
