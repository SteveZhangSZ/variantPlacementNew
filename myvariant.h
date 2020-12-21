#include <cstddef> //for std::size_t
#include <exception> //for std::exception to make bad_variant_access
#include <functional> //For "visit" function
#include <initializer_list> //needed for some function overloads
#ifdef PRINTSTUFF
    #include <iostream>
#endif
#include <new> //placement new
#include <type_traits> //need lots of traits

#ifndef SZVARIANT
#define SZVARIANT

#include "otherIdxSeq.h"

namespace szvar{

struct bad_variant_access : public std::exception{
    std::size_t attemptIdx{0}, realIdx{0};
    bad_variant_access() = default;
    bad_variant_access(std::size_t att, std::size_t real) : attemptIdx{att}, realIdx{real}{}
    const char * what() const throw()
    {
        #ifdef PRINTSTUFF
        std::cout << "Inside bad_variant_access what(), Attempt: " << attemptIdx << ' ' << "Real: " << realIdx << ' ';
        #endif
    	return "Accessed inactive variant member";
    }
};

struct monostate{
    friend constexpr bool operator==(monostate, monostate) noexcept { return true; }
    friend constexpr bool operator!=(monostate, monostate) noexcept { return false; }
    friend constexpr bool operator<(monostate, monostate) noexcept { return false; }
    friend constexpr bool operator>(monostate, monostate) noexcept { return false; }
    friend constexpr bool operator<=(monostate, monostate) noexcept { return true; }
    friend constexpr bool operator>=(monostate, monostate) noexcept { return true; }
};
struct valuelessCtor{};
template<typename...> class variantImpl;

template<class... Ts> using variant = variantImpl<otherIdxSeq::make_index_sequence<sizeof...(Ts)>, Ts...>;
//variant alternative
template<std::size_t Idx, class T> struct variant_alternative{
    typedef T type;
    constexpr static std::size_t theIdx = Idx;
    constexpr std::size_t getIdx() const noexcept{return Idx;}
};

template<std::size_t getIdx, class U> 
constexpr variant_alternative<getIdx, U> getLeaf(variant_alternative<getIdx, U>*) {
    return variant_alternative<getIdx, U>{};
}

template<class U, std::size_t getIdx> constexpr std::size_t theLeafGetter(variant_alternative<getIdx, U>*){
    return variant_alternative<getIdx, U>::theIdx;
}

template <std::size_t I, class... Types>
struct variant_alternative<I, variant<Types...>>{
    typedef typename decltype(getLeaf<I>(static_cast<variant<Types...>*>(0)))::type type;
};

template <size_t I, class T>
using variant_alternative_t = typename variant_alternative<I, T>::type;
//end variant alternative
//variant_size
template <class... T>
struct variant_size;

template <class... Types>
struct variant_size<variant<Types...>>
: std::integral_constant<std::size_t, sizeof...(Types)> { };

template<std::size_t... Idx, typename... Ts>
struct variant_size< variantImpl<otherIdxSeq::index_sequence<Idx...>, Ts...>& > : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <class T> struct variant_size<const T> : std::integral_constant<std::size_t, 1>{};
template <class T> struct variant_size<volatile T> : std::integral_constant<std::size_t, 1>{};
template <class T> struct variant_size<const volatile T> : std::integral_constant<std::size_t, 1>{};
//end variant size


//template<class... Ts> struct variant : variantImpl<otherIdxSeq::make_index_sequence<sizeof...(Ts)>, Ts...>{};


template<std::size_t... Idx, typename... Ts>
class variantImpl<otherIdxSeq::index_sequence<Idx...>, Ts...> :public variant_alternative<Idx,Ts>...{
    static_assert( !(std::is_same<void, typename std::remove_cv<Ts>::type>::value || ...) , "No void in variant!");
    typedef variantImpl<otherIdxSeq::index_sequence<Idx...>, Ts...> typeOfThisVariant;
    //member variables
    //typeIndex always between [static_cast<std::size_t>(-1),  ]
    
    std::size_t typeIndex{0}; //if == -1, then no obj is stored
    alignas(([](){
        std::size_t biggestAlign = 0;
        return ((biggestAlign < alignof(Ts) ? biggestAlign = alignof(Ts) : biggestAlign),...);        
    }())) std::byte buffer[([](){ 
        std::size_t biggestSize = 0;
        return ((biggestSize < sizeof(Ts) ? biggestSize = sizeof(Ts) : biggestSize),...);        
    }())]{};

    //End member variables

    explicit constexpr variantImpl(valuelessCtor) : typeIndex{static_cast<std::size_t>(-1)}{}
    public:
    typedef otherIdxSeq::index_sequence<Idx...> theIdxSeq;
    //constructors

    constexpr variantImpl(){
        //Default construct Ts' first element
        ::new(&buffer)variant_alternative_t<0, variant<Ts...>>{};
    }
    template< class T >
    constexpr variantImpl(T&& t) : typeIndex{theLeafGetter<T>(static_cast<typeOfThisVariant*>(0))}{
        ::new(&buffer)T{static_cast<T&&>(t)};
    }
    //Begin public friend functions
    
    template< std::size_t I, class... Types >
    friend constexpr variant_alternative_t<I, variant<Types...>>& get(variant<Types...>& v );

    template< std::size_t I, class... Types >
    friend constexpr variant_alternative_t<I, variant<Types...>>&& get(variant<Types...>&& v );

    template< std::size_t I, class... Types >
    friend constexpr variant_alternative_t<I, variant<Types...>>& get_unchecked(variant<Types...>& v );

    template< std::size_t I, class... Types >
    friend constexpr variant_alternative_t<I, variant<Types...>>&& get_unchecked(variant<Types...>&& v);

    template< class T, class... Types >
    friend constexpr std::add_pointer_t<T> get_if( variant<Types...>* pv );

    template<class T, class... Types>
    friend constexpr bool holds_alternative(const variant<Types...>& theV);

    //End public friend functions
    template<class T, class... Args>
    constexpr T& emplace(Args&&... args){
        if constexpr(!(std::is_trivially_destructible<Ts>::value && ...)){ //If some type(s) aren't trivially destructible
            static_cast<void>( ((typeIndex == theLeafGetter<Ts>(static_cast<typeOfThisVariant*>(0)) ? 
            (typeIndex = static_cast<std::size_t>(-1), //After destroying, set typeIndex to -1 to show it's valueless
            static_cast<Ts*>(static_cast<void*>(&buffer))->~Ts(), true) : false ) || ...));
        }
        
        //If throws, then typeIndex isn't updated to T's index
        (::new(&buffer)T{static_cast<Args&&>(args)...}); 

        typeIndex = theLeafGetter<T>(static_cast<typeOfThisVariant*>(0));
        
        //static_assert((std::is_same<T,Ts>::value + ...) == 1, "T doesn't appear exactly once in variant");        
        return *static_cast<T*>(static_cast<void*>(&buffer));
    }
    template<std::size_t I, class... Args>
    constexpr variant_alternative_t<I, variant<Ts...>>& emplace(Args&&... args){
        static_assert(I < sizeof...(Idx), "I is too big, therefore absent from this variant");
        if constexpr(!(std::is_trivially_destructible<Ts>::value && ...)){
            static_cast<void>( ((typeIndex == theLeafGetter<Ts>(static_cast<typeOfThisVariant*>(0)) ? 
            (typeIndex = static_cast<std::size_t>(-1), //After destroying, set typeIndex to -1 to show it's valueless
            static_cast<Ts*>(static_cast<void*>(&buffer))->~Ts(), true) : false ) || ...));
        }
        typeIndex = I;
        return *(::new(&buffer)variant_alternative_t<I, variant<Ts...>>{static_cast<Args&&>(args)...});
    }
    constexpr std::size_t index() const noexcept{return typeIndex;}
    constexpr bool valueless_by_exception() const noexcept{return typeIndex == static_cast<std::size_t>(-1);}
    //constexpr std::size_t getNumTypes() const noexcept{return sizeof...(Ts);}

    template<std::size_t... myIdx, typename... myTs>
    friend constexpr bool operator==(const variantImpl<otherIdxSeq::index_sequence<myIdx...>, myTs...>& v, const variantImpl<otherIdxSeq::index_sequence<myIdx...>, myTs...>& w);

    
};
//Friend functions

template< std::size_t I, class... Types >
constexpr variant_alternative_t<I, variant<Types...>>& get(variant<Types...>& v){
    static_assert(I < sizeof...(Types), "I is too big, therefore absent from this variant");
    if(I == v.typeIndex){return *static_cast<variant_alternative_t<I, variant<Types...>>*>(static_cast<void*>(&v.buffer));}
    throw bad_variant_access{I, v.typeIndex};
}
template< std::size_t I, class... Types >
constexpr variant_alternative_t<I, variant<Types...>>&& get(variant<Types...>&& v ){
    static_assert(I < sizeof...(Types), "I is too big, therefore absent from this variant");
    if(I == v.typeIndex){return static_cast<variant_alternative_t<I, variant<Types...>>&&>(*static_cast<variant_alternative_t<I, variant<Types...>>*>(static_cast<void*>(&v.buffer)));}
    throw bad_variant_access{I, v.typeIndex};
}

template< std::size_t I, class... Types >
constexpr variant_alternative_t<I, variant<Types...>>& get_unchecked(variant<Types...>& v){ //TODO: make r value version
    static_assert(I < sizeof...(Types), "I is too big, therefore absent from this variant");
    return *static_cast<variant_alternative_t<I, variant<Types...>>*>(static_cast<void*>(&v.buffer));
}
template< std::size_t I, class... Types >
constexpr variant_alternative_t<I, variant<Types...>>&& get_unchecked(variant<Types...>&& v){
    return static_cast<variant_alternative_t<I, variant<Types...>>&&>(*static_cast<variant_alternative_t<I, variant<Types...>>*>(static_cast<void*>(&v.buffer)));
}
template< class T, class... Types >
constexpr std::add_pointer_t<T> get_if( variant<Types...>* pv ){
    return (theLeafGetter<T>(static_cast<variant<Types...>*>(0)) == pv->typeIndex ? 
    static_cast<std::add_pointer_t<T>>(static_cast<void*>(&(pv->buffer))) : 0);
}

template<class T, class... Us>
constexpr bool holds_alternative(const variant<Us...>& theV){
    return theLeafGetter<T>(static_cast<variant<Us...>*>(0)) == theV.typeIndex;
}
template<typename ... Nums> constexpr std::size_t getNextVarIdx(const std::size_t nth,const Nums... theNum){
    std::size_t count = 0, result = 0;
    static_cast<void>(((count == nth ? (result = theNum, true) : (++count,false) ) || ... ));
    return result;
}

template<std::size_t whichVariantIdx,
std::size_t LowerBound,
std::size_t... indexSeqNums, //0,1,2... [0,sizeof...(vs))
std::size_t... Indices, //for get_unchecked
class F,
typename... Vs> constexpr decltype(auto) //std::invoke_result_t<F, variant_alternative_t<0,std::remove_reference_t<Vs>>...> 
visitOneIfCheck(const otherIdxSeq::index_sequence<indexSeqNums...> theISN,
otherIdxSeq::index_sequence<Indices...>, //Contains upper-bounds, then later the active member indices.
//First invocation, Indices... == number of types in each of the variants. sizeof...(Indices) == sizeof...(vs)
const std::size_t numOfMatches,
F&& f, Vs&& ... vs){
    constexpr std::size_t theUpperBound = getNextVarIdx(whichVariantIdx,Indices... );
    if constexpr(LowerBound == theUpperBound){
        if( numOfMatches == sizeof...(vs)){
            return std::invoke(static_cast<F&&>(f), 
            get_unchecked<(indexSeqNums == whichVariantIdx ? LowerBound : Indices)>(static_cast<Vs&&>(vs))...);
        } else {
            if constexpr(whichVariantIdx != sizeof...(vs)){
                return visitOneIfCheck<whichVariantIdx + 1, 0>(theISN, 
                otherIdxSeq::index_sequence<(indexSeqNums == whichVariantIdx ? LowerBound : Indices)...>{},
                numOfMatches + 1,
                static_cast<F&&>(f), static_cast<Vs&&>(vs)...   ) ;
            } else {
                throw szvar::bad_variant_access{}; //Change to szvar::bad_variant_access{} when done debugging
            }
        }
    } else {
        constexpr std::size_t ceiling = //(LowerBound/2) + (theUpperBound/2) + //Preventing overflow from adding first
        ((LowerBound + theUpperBound) / 2) +
        ((LowerBound & 1) ^ (theUpperBound & 1)); //Check if one number is odd. if one is, then the number is a decimal, so bump it up
        const std::size_t varArgIdx = getNextVarIdx(whichVariantIdx, vs.index()...);
        if(ceiling > varArgIdx){
            return visitOneIfCheck<whichVariantIdx, LowerBound>(theISN, 
            otherIdxSeq::index_sequence<(indexSeqNums == whichVariantIdx ? ceiling - 1 : Indices)...>{},numOfMatches,
            static_cast<F&&>(f), static_cast<Vs&&>(vs)...  );
        } else {
            return visitOneIfCheck<whichVariantIdx, ceiling>(theISN, 
            otherIdxSeq::index_sequence<(indexSeqNums == whichVariantIdx ? theUpperBound : Indices)...>{},numOfMatches, 
            static_cast<F&&>(f), static_cast<Vs&&>(vs)...  );
        }
    }
}


namespace forSingleVisitor{
    class emptyClass{};
    #define CELLCTORMACRO emptyClass forConstexprCtor;\
    T theNode;\
    constexpr Cell() : forConstexprCtor{}{};\
    template<typename... Args>\
    constexpr Cell(Args&&... args) : theNode(static_cast<Args&&>(args)...){}

    template<class,bool triviallyDestructible> union Cell;
    template<class T> union Cell<T,true> {
        CELLCTORMACRO
    };
    template<class T> union Cell<T,false> {
        CELLCTORMACRO
        ~Cell(){
            theNode.~T(); //By end of fold expression in singleVisitor, theNode will be the active member
        }
    };
    #undef CELLCTORMACRO
}


template <typename F, typename Vs, std::size_t... indexSeqNums>
constexpr decltype(auto) //std::invoke_result_t<F, variant_alternative_t<0,std::remove_reference_t<Vs>>> 
singleVisitor(F&& f, otherIdxSeq::index_sequence<indexSeqNums...>, Vs&& vs){
    typedef std::invoke_result_t<F, variant_alternative_t<0,std::remove_reference_t<Vs>>> returnType;
    if constexpr(std::is_void<returnType>::value){
        static_cast<void>( ((vs.index() == indexSeqNums ? ( std::invoke(static_cast<F&&>(f), get_unchecked<indexSeqNums>(static_cast<Vs&&>(vs) ) ), true )
        : false ) || ...) );
    }
    else if constexpr(std::is_trivially_copy_assignable<returnType>::value){
        forSingleVisitor::Cell<returnType,std::is_trivially_destructible<Vs>::value> hasResult;
        ( (vs.index() == indexSeqNums ? 
        (hasResult = forSingleVisitor::Cell<returnType,std::is_trivially_destructible<Vs>::value>{std::invoke(static_cast<F&&>(f), get_unchecked<indexSeqNums>(static_cast<Vs&&>(vs) ) )}, true ) 
        : false) || ... );
        return hasResult.theNode;
    }
    else{ //else if default constructible. Then copy assign.
        returnType hasResult;
        ( (vs.index() == indexSeqNums ? 
        (hasResult = std::invoke(static_cast<F&&>(f), get_unchecked<indexSeqNums>(static_cast<Vs&&>(vs))), true ) 
        : false) || ... );
        return hasResult;
    }
}


template <typename F, typename ... Vs>
//std::invoke_result_t<F, variant_alternative_t<0,std::remove_reference_t<Vs>>> //return type
constexpr decltype(auto) visit(F&& f, Vs&& ... vs){
    if constexpr(sizeof...(Vs) == 0){
        return std::invoke(static_cast<F&&>(f));
    } 
    else if constexpr(sizeof...(Vs) == 1){
        typedef std::invoke_result_t<F, variant_alternative_t<0,std::remove_reference_t<Vs>>... > returnType;
        static_assert(std::is_void<returnType>::value || std::is_trivially_copy_assignable<returnType>::value
        || (std::is_default_constructible<returnType>::value && std::is_copy_assignable<returnType>::value),
        "Function's return type must be void, trivially copy assignable, or default constructible and copy assignable" );
        return singleVisitor(static_cast<F&&>(f), (typename Vs::theIdxSeq{})..., vs...);
    }
    else{
        return visitOneIfCheck<0,0>(otherIdxSeq::make_index_sequence<sizeof...(vs)>{},
        otherIdxSeq::index_sequence<(variant_size<Vs>::value - 1)...>{},
        0,static_cast<F&&>(f), static_cast<Vs&&>(vs)...);
    }
}


//End friend functions
//Member functions
template<std::size_t... Idx, typename... Ts>
//template<class... Ts>
constexpr bool operator==(const variantImpl<otherIdxSeq::index_sequence<Idx...>, Ts...>& v, const variantImpl<otherIdxSeq::index_sequence<Idx...>, Ts...>& w){    
    return v.valueless_by_exception() || ((v.index() == w.index()) && [&v,&w](){
        bool theBoolAnswer = false;
        static_cast<void>(((v.index() == Idx ? ( (theBoolAnswer = *static_cast<const typename variant_alternative<Idx,variant<Ts...>>::type*>( static_cast<const void*>(&(v.buffer)) )
        == *static_cast<const typename variant_alternative<Idx,variant<Ts...>>::type*>( static_cast<const void*>(&(w.buffer)) ) ), true ) : false) || ...));
        return theBoolAnswer;
    }());
}
} //namespace szvar
#endif //SZVARIANT