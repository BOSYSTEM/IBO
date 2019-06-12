#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/transaction.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/types.h>
#include "../Common/events.hpp"

using namespace eosio;
using std::string;
using std::vector;


#define EMIT_CONVERSION_EVENT( printout, args, line) \
    START_EVENT("\nconversion", "1.1") \
    EVENTKV("\n", printout) \
    EVENTKV(":", args) \
    END_EVENT()

vector<string> split(const string& str, const string& delim)
{
    vector<string> tokens;
    size_t prev = 0, pos = 0;

    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}


CONTRACT ExChange : public eosio::contract{
    using contract::contract;

    TABLE assets_t {
        name token_contract;
        name ban_account;
        asset user_currency;
        name pro_account;
        asset pro_currency;
        name forward_account;
        asset forward_currency;
        uint64_t min_quantity;
        uint64_t exchangerd;
        uint64_t open_buytime;
        uint64_t open_extracttime;
        bool buy_switch;
        bool extract_switch;

        uint64_t primary_key() const {return user_currency.symbol.code().raw();}
    };

    TABLE tgeuser_t {
        asset user_currency;
        asset user_totbuy;

        uint64_t primary_key() const {return user_currency.symbol.code().raw();}
    };

    TABLE tgepro_t {
        asset pro_currency;
        asset pro_totbuy;
        uint32_t time;

        uint64_t primary_key() const {return pro_currency.symbol.code().raw();}
    };

    TABLE tgeforward_t {
        asset forward_currency;
        asset forward_totbuy;
        uint32_t time;

        uint64_t primary_key() const {return forward_currency.symbol.code().raw();}
    };

    TABLE bancorset_t {
        name  token_contract;
        asset bancor_balance;
        asset bancor_supply;   
        uint64_t bancor_radio;

        uint64_t primary_key() const {return bancor_supply.symbol.code().raw();}
    };

    TABLE virtualset_t {
        asset tokentype;
        name virtua_tokcontract;
        asset virtua_currency;
        asset virtua_totbuy;

        uint64_t primary_key() const {return tokentype.symbol.code().raw();}
    };


    TABLE virtualtok_t {
        asset virtua_currency;
    
        uint64_t primary_key() const {return virtua_currency.symbol.code().raw();}
    };   


    TABLE switch_t {
        bool switch_lock;

        EOSLIB_SERIALIZE(switch_t, (switch_lock))
    };

    TABLE whitelist_t{
        name whitename;
        uint64_t primary_key() const { return whitename.value; } 
    };

    typedef eosio::multi_index<name("assets"), assets_t> assets;
    typedef eosio::multi_index<name("tgeusers"), tgeuser_t> tgeusers;
    typedef eosio::multi_index<name("tgepros"), tgepro_t> tgepros;
    typedef eosio::multi_index<name("tgeforwards"), tgeforward_t> tgeforwards;
    typedef eosio::multi_index<name("bancorsets"), bancorset_t> bancorsets;
    typedef eosio::multi_index<name("virtualsets"), virtualset_t> virtualsets;
    typedef eosio::multi_index<name("virtualtoks"), virtualtok_t> virtualtoks;
    typedef eosio::singleton<name("switchs"), switch_t> switchs;
    typedef eosio::multi_index<name("switchs"), switch_t> dummy_for_abi;
    typedef eosio::multi_index<name("whitelists"), whitelist_t> whitelists; 

public:
    ACTION addassets(name token_contract,name ban_account,asset user_currency,name pro_account,asset pro_currency,name forward_account,asset forward_currency,
    uint64_t min_quantity,uint64_t exchangerd,uint64_t open_buytime,uint64_t open_extracttime,bool buy_switch,bool extract_switch,name executor);
    ACTION addbancor(name token_contract,asset bancor_supply,asset bancor_balance,uint64_t bancor_radio,name executor);
    ACTION addvirtual(asset tokentype,name virtua_tokcontract,asset virtua_currency,name executor);
    ACTION resettime(string symbol,uint64_t opentime,uint64_t closetime,name executor);
    ACTION rstblswitch(string symbol,bool buyswitch,bool sellswitch,name executor);
    ACTION initswitch(bool switch_lock,name executor);
    ACTION resetswitch(bool switch_lock,name executor);
    ACTION addwhitelist(name whitename);
    ACTION inlinetransf(name contract, name from, name to, asset quantity, string memo);
    void transfer(name from, name to, asset quantity, string memo);

private:
    void Identification_status(name executor);
    void Transfer_Mortage(name from, asset quantity,string shadow_recieve,string exchange_symbol,name code);
    void Transfer_Extract(name from, asset quantity,string shadow_recieve,string exchange_symbol,name code);
    void Transfer_ProExtract(name from, asset quantity,string shadow_recieve,string exchange_symbol,string exchange_number,name code);
    void Transfer_ForwardExtract(name from, asset quantity,string shadow_recieve,string exchange_symbol,string exchange_number,name code);
    ACTION forwardget(name forward_account,string exchange_symbol);

    asset get_balance(name contract, name owner, symbol_code sym);
    asset get_supply(name contract, symbol_code sym);
    string Json_Transformation(string type,string shadow_recieve,string symbol);
    asset stringToasset(string num,string tosymbol)
    {
        bool minus = false;      //标记是否是负数  
        string real = num;       //real表示num的绝对值
        if (num.at(0) == '-')
        {
            minus = true;
            real = num.substr(1, num.size()-1);
        }

        char c;
        int i = 0;
        double result = 0.0 , dec = 10.0;
        bool isDec = false;       //标记是否有小数
        unsigned long size = real.size();
        while(i < size)
        {
            c = real.at(i);
            if (c == '.')
            {//包含小数
                isDec = true;
                i++;
                continue;
            }
            if (!isDec) 
            {
                result = result*10 + c - '0';
            }
            else
            {//识别小数点之后都进入这个分支
                result = result + (c - '0')/dec;
                dec *= 10;
            }
            i++;
        }

        if (minus == true) {
            result = -result;
        }
        asset sendresult = asset(int(result * 10000),symbol(tosymbol,4));
        return sendresult;
    }
};