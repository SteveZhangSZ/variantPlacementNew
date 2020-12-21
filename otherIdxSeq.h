#include <cstdint>
#ifndef OTHERIDXSEQ
#define OTHERIDXSEQ
namespace otherIdxSeq{ //https://stackoverflow.com/a/20101039
    template< std::size_t ... i >
    struct index_sequence
    {
        typedef std::size_t value_type;
        typedef index_sequence<i...> type;   
        static constexpr std::size_t size() noexcept
        { 
            return sizeof ... (i); 
        }
    };


    // this structure doubles index_sequence elements.
    // s- is number of template arguments in IS.
    template< std::size_t s, typename IS >
    struct doubled_index_sequence;

    template< std::size_t s, std::size_t ... i >
    struct doubled_index_sequence< s, index_sequence<i... > >
    {
        typedef index_sequence<i..., (s + i)... > type;
    };

    // this structure incremented by one index_sequence, iff NEED-is true, 
    // otherwise returns IS
    template< bool NEED, typename IS >
    struct inc_index_sequence;

    template< typename IS >
    struct inc_index_sequence<false,IS>{ typedef IS type; };

    template< std::size_t ... i >
    struct inc_index_sequence< true, index_sequence<i...> >
    {
        typedef index_sequence<i..., sizeof...(i)> type;
    };



    // helper structure for make_index_sequence.
    template< std::size_t N >
    struct make_index_sequence_impl : 
            inc_index_sequence< (N % 2 != 0), 
                    typename doubled_index_sequence< N / 2,
                            typename make_index_sequence_impl< N / 2> ::type
                >::type
        >
    {};

    // helper structure needs specialization only with 0 element.
    template<>struct make_index_sequence_impl<0>{ typedef index_sequence<> type; };

    template< std::size_t N > 
    using make_index_sequence = typename make_index_sequence_impl<N>::type;
}

#endif