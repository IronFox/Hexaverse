#define MAKE_MUTLI_SERIALIZABLE_CLASS(SUPER_DEPTH,THIS_DEPTH)\
	template <M_TYPES##THIS_DEPTH>\
		class MultiSerializable##THIS_DEPTH: public MultiSerializable##SUPER_DEPTH <M_T##SUPER_DEPTH>\
		{\
		protected:\
			typedef MultiSerializable##THIS_DEPTH <M_T##THIS_DEPTH>	Self;\
			typedef MultiSerializable##SUPER_DEPTH <M_T##SUPER_DEPTH>	Super;\
			T##SUPER_DEPTH	var##SUPER_DEPTH;\
		public:\
			virtual	serial_size_t				GetSerialSize(bool exportSize) const	override\
			{\
				return Super::GetSerialSize(true) + var##SUPER_DEPTH.GetSerialSize(true);\
			}\
			virtual	bool						HasFixedSize() const	override\
			{\
				return Super::HasFixedSize() && var##SUPER_DEPTH.HasFixedSize();\
			}\
			virtual	void		Serialize(IWriteStream&outStream, bool exportSize) const	override\
			{\
				Super::Serialize(outStream,true);\
				var##SUPER_DEPTH.Serialize(outStream,true);\
			}\
			virtual	void		Deserialize(IReadStream&inStream, serial_size_t fixedSize)	override\
			{\
				Super::Deserialize(inStream,EmbeddedSize);\
				var##SUPER_DEPTH.Deserialize(inStream,EmbeddedSize);\
			}\
			void	Copy(const Self&other)\
			{\
				Super::Copy(other); var##SUPER_DEPTH = other.var##SUPER_DEPTH;\
			}\
			void		swap(Self&other)	{var##SUPER_DEPTH.swap(other.var##SUPER_DEPTH);Super::swap(*this);}\
		};

				//return	Super::deserialize(inStream,EmbeddedSize) && var##SUPER_DEPTH.deserialize(inStream,EmbeddedSize);
