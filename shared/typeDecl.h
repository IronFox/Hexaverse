#ifndef typeDeclH
#define typeDeclH

#include <memory>

#define DECLARE_POINTER_CLASS(T)	class T; typedef std::shared_ptr<T> P##T; typedef std::weak_ptr<T>	W##T; typedef std::shared_ptr<const T> PConst##T;


#endif
