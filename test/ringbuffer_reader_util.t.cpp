#include <MarketData.hpp>
#include <MarketDataShfeMulticast.hpp>
#include <MarketDataCtpBroker34.hpp>
#include <PrivateData.hpp>

#include <Util/Ringbuffer.hpp>
#include <Util/Spdlog.hpp>
#include <Util/Util.hpp>
#include <Util/PrintStructs.hpp>

#include <Api/ZceLevel2.hpp>
#include <Api/Level2.hpp>
#include <Api/XSpeed.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

// structs from MarketDataShfeMulticast.hpp
BOOST_FUSION_ADAPT_STRUCT(
    InstrumentTime,
    InstrumentID,
    UpdateTime,
    UpdateMillisec)

BOOST_FUSION_ADAPT_STRUCT(
    HighLow,
    UpperLimitPrice,
    LowerLimitPrice)

BOOST_FUSION_ADAPT_STRUCT(
    PriceVolume,
    LastPrice,
    Volume,
    Turnover,
    OpenInterest)

BOOST_FUSION_ADAPT_STRUCT(
    AskBid,
    BidPrice1,
    BidVolume1,
    AskPrice1,
    AskVolume1)

BOOST_FUSION_ADAPT_STRUCT(
    IncQuotaData,
    InstTime,
    HighLowInst,
    PriVol,
    AskBidInst)

// structs from PrivateData.hpp
BOOST_FUSION_ADAPT_STRUCT(
    MdSignature,
    bestBidLargep,
    bestAskLargep,
    mdVolume,
    bestBidSize,
    bestAskSize)

BOOST_FUSION_ADAPT_STRUCT(
    TradeDetail,
    mdSig,
    accountCode,
    secIntId,
    orderId,
    globalOrderId,
    exchangeOrderId,
    internalOrderId,
    signedSize,
    insertPrice,
    matchPrice,
    ticksAwayAtInsert,
    clockAtTrade,
    lastMdSp,
    lastMdMinSp,
    unsignedRemaining)

BOOST_FUSION_ADAPT_STRUCT(
    OrderRecap,
    mdSig,
    secIntId,
    orderId,
    signedSize,
    price,
    lastMdSp,
    isTraded,
    isLast)

// structs from MarketData.hpp
BOOST_FUSION_ADAPT_STRUCT(
    MarketData,
    _level,
    __secName,
    _bidPrice,
    _askPrice,
    _bidSize,
    _askSize,
    _volume,
    _oi,
    _lastPriceLargep,
    _turnover,
    _lastPriceSmallp,
    _lastSize,
    _tick,
    _stale,
    _mdSource,
    _clockAtArrival,
    _updateTime,
    _updateMillisec)

BOOST_FUSION_ADAPT_STRUCT(
    zcelevel2_api::ZCEL2QuotSnapshotField,
    ContractID,
    ContractIDType,
    PreSettle,
    PreClose,
    PreOpenInterest,
    OpenPrice,
    HighPrice,
    LowPrice,
    LastPrice,
    BidPrice,
    AskPrice,
    BidLot,
    AskLot,
    TotalVolume,
    OpenInterest,
    ClosePrice,
    SettlePrice,
    AveragePrice,
    LifeHigh,
    LifeLow,
    HighLimit,
    LowLimit,
    TotalBidLot,
    TotalAskLot,
    TimeStamp)

BOOST_FUSION_ADAPT_STRUCT(
    level2_api::DFITC_L2::MDBestAndDeep,
    Type,
    Length,
    Version,
    Time,
    Exchange,
    Contract,
    SuspensionSign,
    LastClearPrice,
    ClearPrice,
    AvgPrice,
    LastClose,
    Close,
    OpenPrice,
    LastOpenInterest,
    OpenInterest,
    LastPrice,
    MatchTotQty,
    Turnover,
    RiseLimit,
    FallLimit,
    HighPrice,
    LowPrice,
    PreDelta,
    CurrDelta,
    BuyPriceOne,
    BuyQtyOne,
    BuyImplyQtyOne,
    SellPriceOne,
    SellQtyOne,
    SellImplyQtyOne,
    GenTime,
    LastMatchQty,
    InterestChg,
    LifeLow,
    LifeHigh,
    Delta,
    Gamma,
    Rho,
    Theta,
    Vega,
    TradeDate,
    LocalDate)

BOOST_FUSION_ADAPT_STRUCT(
    ctpbroker34_api::CThostFtdcDepthMarketDataField,
    TradingDay,
    InstrumentID,
    ExchangeID,
    ExchangeInstID,
    LastPrice,
    PreSettlementPrice,
    PreClosePrice,
    PreOpenInterest,
    OpenPrice,
    HighestPrice,
    LowestPrice,
    Volume,
    Turnover,
    OpenInterest,
    ClosePrice,
    SettlementPrice,
    UpperLimitPrice,
    LowerLimitPrice,
    PreDelta,
    CurrDelta,
    UpdateTime,
    UpdateMillisec,
    BidPrice1,
    BidVolume1,
    AskPrice1,
    AskVolume1,
    BidPrice2,
    BidVolume2,
    AskPrice2,
    AskVolume2,
    BidPrice3,
    BidVolume3,
    AskPrice3,
    AskVolume3,
    BidPrice4,
    BidVolume4,
    AskPrice4,
    AskVolume4,
    BidPrice5,
    BidVolume5,
    AskPrice5,
    AskVolume5,
    AveragePrice,
    ActionDay)

BOOST_FUSION_ADAPT_STRUCT(
    level2_api::DFITC_L2::MDOrderStatistic,
    Type,
    Len,
    ContractID,
    TotalBuyOrderNum,
    TotalSellOrderNum,
    WeightedAverageBuyOrderPrice,
    WeightedAverageSellOrderPrice)

BOOST_FUSION_ADAPT_STRUCT(
    xspeed_api::DFITCDepthMarketDataField,
    tradingDay,
    instrumentID,
    exchangeID,
    exchangeInstID,
    lastPrice,
    preSettlementPrice,
    preClosePrice,
    preOpenInterest,
    openPrice,
    highestPrice,
    lowestPrice,
    Volume,
    turnover,
    openInterest,
    closePrice,
    settlementPrice,
    upperLimitPrice,
    lowerLimitPrice,
    preDelta,
    currDelta,
    UpdateTime,
    UpdateMillisec,
    BidPrice1,
    BidVolume1,
    AskPrice1,
    AskVolume1,
    BidPrice2,
    BidVolume2,
    AskPrice2,
    AskVolume2,
    BidPrice3,
    BidVolume3,
    AskPrice3,
    AskVolume3,
    BidPrice4,
    BidVolume4,
    AskPrice4,
    AskVolume4,
    BidPrice5,
    BidVolume5,
    AskPrice5,
    AskVolume5,
    AveragePrice,
    XSpeedTime)

using namespace std;
using namespace std::literals;

namespace
{
struct test_field_printer_accumulator
{
    template <typename T>
    void operator()(char const * name, T const & val) const
    {
        accumulator += fmt::format("{}={} ", name, val);
    }

    mutable std::string accumulator{};
};

struct test_field_printer
{
    template <typename T>
    typename std::enable_if<!util::is_fusion_sequence<T>::value>::type
        operator()(char const * name, T const & val) const
    {
        fmt::print("  {:25} = {}\n", name, val);
    }

    template <typename T>
    typename std::enable_if<util::is_fusion_sequence<T>::value>::type
        operator()(char const * name, T const & val) const
    {
        test_field_printer_accumulator t;
        util::for_each_field(val, t);
        (*this)(name, "[ " + t.accumulator + "]");
    }

    template <typename T, std::size_t N>
    typename std::enable_if<!std::is_same<char, typename std::remove_cv<T>::type>::value>::type
    operator()(char const * name, T (&arr)[N]) const
    {
        std::ostringstream out;
        out << '[';

        std::size_t i = 0;
        for (auto && elem : arr)
        {
            out << elem;
            if (++i != N)
                out << ", ";
        }

        out << ']';
        (*this)(name, out.str());
    }
};

template <typename T>
void read_ring_buffer_file(char const * file_name, char const * type_name)
{
    tsq::ring_buffer_reader<T> rb{file_name};
    fmt::print("file             : {}\n"
               "number of records: {}\n"
               "record type      : {}\n",
               file_name,
               rb.size(),
               type_name);

    int pos = 0;
    for (auto && rec : rb)
    {
        fmt::print("record {}:\n", pos++);
        util::for_each_field(rec, test_field_printer{});
    }
}

struct struct_reader_rec
{
    char const *                                     struct_name;
    std::function<void (char const *, char const *)> reader_func;
};

array<struct_reader_rec, 10> ring_buffer_readers = {
    "zcelevel2",     read_ring_buffer_file<zcelevel2_api::ZCEL2QuotSnapshotField>,
    "pmc",           read_ring_buffer_file<MarketData>,
    "level2",        read_ring_buffer_file<level2_api::DFITC_L2::MDBestAndDeep>,
    "level2ctp",     read_ring_buffer_file<ctpbroker34_api::CThostFtdcDepthMarketDataField>,
    "level3",        read_ring_buffer_file<MarketData>,
    "ostat",         read_ring_buffer_file<level2_api::DFITC_L2::MDOrderStatistic>,
    "trades",        read_ring_buffer_file<TradeDetail>,
    "orders",        read_ring_buffer_file<OrderRecap>,
    "xspeed",        read_ring_buffer_file<xspeed_api::DFITCDepthMarketDataField>,
    "shfemulticast", read_ring_buffer_file<IncQuotaData>,
};

}  // anonymous namespace

int main(int argc, char * argv[])
try
{
    if (argc < 3)
    {
        if (argc >= 2 && argv[1] == "-l"s)
        {
            cout << "List of available struct names:\n";
            for (auto && reader : ring_buffer_readers)
                cout << reader.struct_name << endl;
            return 0;
        }

        cerr << fmt::format("Usage:\n"
                            "   {0} ringbuf-file struct-name\n"
                            "   {0} -l  - list all structs\n", argv[0]);
        return 1;
    }

    string input_struct{argv[2]};
    auto pos = std::find_if(begin(ring_buffer_readers),
                            end(ring_buffer_readers),
                            [&] (auto const & reader) {
                                return input_struct == reader.struct_name;
                            });

    if (pos == end(ring_buffer_readers))
    {
        cerr << fmt::format("struct '{}' is not in the available structs list (-l)\n");
        return 2;
    }

    pos->reader_func(argv[1], argv[2]);
}
catch (exception & e)
{
    cerr << "error: " << e.what() << endl;
    return -1;
}
