#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/transaction.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/types.h>

using namespace eosio;
using std::string;
using std::vector;

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


CONTRACT ShadowUser : public eosio::contract{
    using contract::contract;

public:
    TABLE userinfo_t {
        uint64_t primary;
        name user_account;
        string public_key;
        vector<uint64_t> timetamp;
        vector<uint64_t> nonce;
        bool lock_state;

        uint64_t primary_key() const {return primary;}
    };

    TABLE allowblance_t{
        asset balance;
        name token_contract;
        bool balance_switch;

        uint64_t primary_key() const {return balance.symbol.code().raw();}
    };

    TABLE userbalance_t{
        asset user_balance;
        name token_contract;

        uint64_t primary_key() const {return user_balance.symbol.code().raw();}
    };

    TABLE switch_t {
        bool switch_lock;

        EOSLIB_SERIALIZE(switch_t, (switch_lock))
    };

    TABLE whitelist_t{
        name whitename;
        uint64_t primary_key() const { return whitename.value; } 
    };

    TABLE inputlist_t{
        name inputname;
        uint64_t primary_key() const { return inputname.value; } 
    };

    typedef eosio::multi_index<"userinfos"_n, userinfo_t> userinfos;
    typedef eosio::multi_index<"allowblances"_n, allowblance_t> allowblances;
    typedef eosio::multi_index<"userbalances"_n, userbalance_t> userbalances;
    typedef eosio::singleton<"switchs"_n, switch_t> switchs;
    typedef eosio::multi_index<"switchs"_n, switch_t> dummy_for_abi;
    typedef eosio::multi_index<"whitelists"_n, whitelist_t> whitelists;
    typedef eosio::multi_index<"inputlists"_n, inputlist_t> inputlists;    

    ACTION createuser(name user_account,string public_key,bool lock_state,name executor);
    ACTION addbalance(name token_contract,asset balance,bool balance_switch,name executor);
    ACTION rstpubkey(name user_account,string public_key,name executor);
    ACTION rstuserlock(name user_account,bool lock_state,name executor);
    ACTION stationin(name from,name to,asset quantity,uint64_t timetamp,uint64_t nonce,string AuthSign,string memo,name executor);
    ACTION stationout(name from,name to,asset quantity,uint64_t timetamp,uint64_t nonce,string AuthSign,string memo,name executor);
    ACTION inlinetransf(name contract, name from, name to, asset quantity, string memo);
    ACTION initswitch(bool switch_lock,name executor);
    ACTION resetswitch(bool switch_lock,name executor);
    ACTION addwhitelist(name whitename);
    ACTION rstblswitch(name token_contract,string symbol,bool balance_switch,name executor);


    //***********新添加接口*************
    ACTION addinputlist(name input_name,name executor);
    //********************************

    void transfer(name from, name to, asset quantity, string memo); 

private:
    void ExternalTransfer(name from,asset quantity,name code,string memo);
    void StrToHex(const char lpSrcStr[], unsigned char lpRetBytes[], size_t lpRetSize);
    void Identification_status(name executor);
    void Check_Sign(string Public,string message,string AuthSign);
    asset get_supply(name contract, symbol_code sym);
    asset get_balance(name contract, name owner, symbol_code sym);
};