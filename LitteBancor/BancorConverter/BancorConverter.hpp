#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/singleton.hpp>
#include "../Common/common.hpp"

using namespace eosio;
using std::string;
using std::vector;


#define EMIT_CONVERSION_EVENT( printout, args, line) \
    START_EVENT("\nconversion", "1.1") \
    EVENTKV("\n", printout) \
    EVENTKV(":", args) \
    END_EVENT()


CONTRACT BancorConverter : public eosio::contract {
    using contract::contract;
    public:
        TABLE currencylst_t{
            name     contract;
            asset    currency;
            name     saveaccount;
            name     proaccount;
            uint64_t max_fee;
            uint64_t buy_fee;
            uint64_t buy_baseline;
            uint64_t sell_fee;
            uint64_t sell_baseline;
            uint64_t save_fee;
            bool     enabled;
            uint64_t opentime;
            uint64_t closetime;
            uint64_t primary_key() const { return currency.symbol.code().raw(); }
        };

        TABLE reserve_t {
            asset    tokentype;
            name     contract;
            asset    currency;
            uint64_t ratio;
            uint64_t primary_key() const { return tokentype.symbol.code().raw(); }
        };

        TABLE curreserve_t{
            asset   tokentype;
            asset   currency;
            uint64_t primary_key() const { return tokentype.symbol.code().raw(); }
        };

        TABLE whitelist_t{
            name whitename;
            uint64_t primary_key() const { return whitename.value; } 
        };

        TABLE switch_t {
            bool switch_lock;
            EOSLIB_SERIALIZE(switch_t, (switch_lock))
        };

        typedef eosio::multi_index<"reserves"_n, reserve_t> reserves;
        typedef eosio::multi_index<"curreserves"_n, curreserve_t> curreserves;
        typedef eosio::multi_index<"whitelists"_n, whitelist_t> whitelists;
        typedef eosio::multi_index<"currencylsts"_n, currencylst_t> currencylsts;
        typedef eosio::singleton<"switchs"_n, switch_t> switchs;
        typedef eosio::multi_index<"switchs"_n, switch_t> dummy_for_abi;

        ACTION addcurrency(name contract,asset currency,name saveaccount,name proaccount,uint64_t max_fee,uint64_t buy_fee,uint64_t buy_baseline
        ,uint64_t sell_fee,uint64_t sell_baseline,uint64_t save_fee,uint64_t opentime,uint64_t closetime,bool enabled,name executor);
        ACTION addreserve(asset tokentype,name contract,asset currency,uint64_t ratio,name executor);
        ACTION switchbc(string symbol,bool enabled,name executor);
        ACTION resetfee(string symbol,uint64_t buy_fee,uint64_t sell_fee,name executor);
        ACTION resetline(string symbol,uint64_t buy_baseline,uint64_t sell_baseline,name executor);
        ACTION resetsavfee(string symbol,uint64_t save_fee,name executor);
        ACTION inlinetransf(name contract, name from, name to, asset quantity, string memo);
        ACTION resettime(string symbol,uint64_t opentime,uint64_t closetime,name executor);
        ACTION addwhitelist(name whitename);
        ACTION initswitch(bool switch_lock,name executor);
        ACTION resetswitch(bool switch_lock,name executor);        
        void transfer(name from, name to, asset quantity, string memo); 

    private:
        void convert(name from, asset quantity,string shadow_recieve,string exchange_symbol,name code);
        asset get_supply(name contract, symbol_code sym);
        asset get_balance(name contract, name owner, symbol_code sym);
        const reserve_t& get_reserve(uint64_t fromname,uint64_t toname);
        string Json_Transformation(string type,string shadow_recieve,string symbol);
        void Identification_status(name executor);
        void recharge(asset quantity,string exchange_symbol,name code);
};
