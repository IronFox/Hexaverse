#ifndef sharedCastH
#define sharedCastH


template <typename T1>
	class To
	{
	public:
		template <typename T0>
			inline static std::shared_ptr<T1>	Cast(const std::shared_ptr<T0>&element)
			{
				return std::static_pointer_cast<T1,T0>(element);
			}
		template <typename T0>
			inline static std::shared_ptr<T1>	DynamicCast(const std::shared_ptr<T0>&element)
			{
				return std::dynamic_pointer_cast<T1,T0>(element);
			}



	};


#endif
