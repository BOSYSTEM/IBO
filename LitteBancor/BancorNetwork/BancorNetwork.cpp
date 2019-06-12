#include "./BancorNetwork.hpp"
#include "../Common/common.hpp"

using namespace eosio;

void BancorNetwork::transfer(name from, name to, asset quantity, string memo) {
    if (to != _self)
        return;
 
    //auto a = extended_asset(, code);
    
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity in transfer");
    eosio_assert(quantity.amount != 0, "zero quantity is disallowed in transfer");
 
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");
    auto parts = split(memo, ",");
    eosio_assert(parts.size() >= 3, "bad path format");
    auto splitsend = split(parts[0], ":");
    auto splitrc = split(parts[1], ":");
    auto splitsym = split(parts[2],":");

    eosio_assert(splitsend[0] == "send" && splitrc[0] == "recieve" && splitsym[0] == "symbol", "bad path format");

    auto splitrclist =  split(splitrc[1],"|");
    string recieve_name = "";
    splitrclist.size() >=2 ? recieve_name = splitrclist[0]:recieve_name = splitrc[1];
    eosio_assert(recieve_name == from.to_string(), "splitrc error");
 
    auto convert_contract = name(splitsend[1]);
      
    action(
         permission_level{ _self, name("active") },
         _code, name("transfer"),
         std::make_tuple(_self, convert_contract, quantity, memo)
    ).send();
}

ACTION BancorNetwork::init() {
    require_auth(_self);
}

extern "C" {
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {

        if (action == "transfer"_n.value && code != receiver) {
            eosio::execute_action( eosio::name(receiver), eosio::name(code), &BancorNetwork::transfer );
        }
        if (code == receiver){
            switch( action ) { 
                EOSIO_DISPATCH_HELPER( BancorNetwork, (init) ) 
            }    
        }
        eosio_exit(0);
    }
}
