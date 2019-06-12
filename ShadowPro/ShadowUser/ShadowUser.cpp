#include "./ShadowUser.hpp"
#include "../micro-ecc-master/uECC.c"


struct account {
    asset    balance;
    uint64_t primary_key() const { return balance.symbol.code().raw(); }
};

TABLE currency_stats {
    asset   supply;
    asset   max_supply;
    name    issuer;
    uint64_t primary_key() const { return supply.symbol.code().raw(); }
};

typedef eosio::multi_index<name("stat"), currency_stats> stats;
typedef eosio::multi_index<name("accounts"), account> accounts;


//添加内部钱包账户
ACTION ShadowUser::createuser(name user_account,string public_key,bool lock_state,name executor){
    Identification_status(executor);
    eosio_assert(public_key.length() == 128,"This public_key is error");
    userinfos userinfo_table(_self, user_account.value);
    auto userinfo = userinfo_table.begin();
    eosio_assert(userinfo == userinfo_table.end(), "This userinfo already defined");

    userinfo_table.emplace(_self,[&](auto &st){
        st.primary = 1;
        st.user_account = user_account;
        st.public_key = public_key;
        st.lock_state = lock_state;
    });
}


//添加钱包所支持的代币
ACTION ShadowUser::addbalance(name token_contract,asset balance,bool balance_switch,name executor){
    Identification_status(executor);
    eosio_assert(balance.is_valid(), "invalid balance");      
    eosio_assert(balance.symbol.is_valid(), "invalid symbol name");

    //******************新添加**********************
    eosio_assert(balance.symbol.precision() == 4, "balance symbol precision mismatch");    
    //*********************************************

    allowblances allowblance_table(_self, _self.value);
    auto allowblance = allowblance_table.find(balance.symbol.code().raw());
    eosio_assert(allowblance == allowblance_table.end(), "This allowblance already defined");

    allowblance_table.emplace(_self,[&](auto &st){
        st.token_contract = token_contract;
        st.balance = balance;
        st.balance_switch = balance_switch;
    });
}

//***********新添加接口:添加外部账户向内部账户转账白名单*******
ACTION ShadowUser::addinputlist(name input_name,name executor){
    Identification_status(executor);

    inputlists inputlist_table(_self, _self.value);
    auto inputlist = inputlist_table.find(input_name.value);
    eosio_assert(inputlist == inputlist_table.end(), "inputlist already defined");

    inputlist_table.emplace(_self, [&](auto& st) {
        st.inputname  = input_name;
    });
}
//******************************************************


//验证接口调用者
void ShadowUser::Identification_status(name executor){
    require_auth(executor);
    whitelists whitelist_table(_self, _self.value);
    auto whitelist = whitelist_table.find(executor.value);
    eosio_assert(whitelist != whitelist_table.end(), "whitelist is not define");
}


//重置某代币的开关
ACTION ShadowUser::rstblswitch(name token_contract,string symbol,bool balance_switch,name executor){
    Identification_status(executor);
    allowblances allowblance_table(_self,_self.value);
    auto allowblance = allowblance_table.find(symbol_code(symbol).raw());
    eosio_assert(allowblance != allowblance_table.end(), "This quantity is not define");
    eosio_assert(allowblance->token_contract == token_contract, "This token_contract is not define");

    allowblance_table.modify(allowblance, eosio::same_payer, [&](auto& st) {
        st.balance_switch = balance_switch;
    });
}


//初始化合约总开关
ACTION ShadowUser::initswitch(bool switch_lock,name executor){
    Identification_status(executor);
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(!switch_exists, "switchs already defined");

    switch_t new_switch;
    new_switch.switch_lock  = switch_lock;
    switch_table.set(new_switch, _self);

}

//重置合约总开关
ACTION ShadowUser::resetswitch(bool switch_lock,name executor){
    Identification_status(executor);
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switchs is not define");

    auto switchs = switch_table.get();
    switchs.switch_lock = switch_lock;
    switch_table.set(switchs, _self);
}


//重置钱包账户公钥
ACTION ShadowUser::rstpubkey(name user_account,string public_key,name executor){
    Identification_status(executor);
    eosio_assert(public_key.length() == 128,"This public_key is error");
    userinfos userinfo_table(_self, user_account.value);
    auto userinfo = userinfo_table.begin();
    eosio_assert(userinfo != userinfo_table.end(), "This userinfo is not defined");

    userinfo_table.modify(userinfo, eosio::same_payer, [&](auto& st) {
        st.public_key = public_key;
    });
}


//重置钱包账户是否为可用
ACTION ShadowUser::rstuserlock(name user_account,bool lock_state,name executor){
    Identification_status(executor);
    userinfos userinfo_table(_self, user_account.value);
    auto userinfo = userinfo_table.begin();
    eosio_assert(userinfo != userinfo_table.end(), "This userinfo is not defined");

    userinfo_table.modify(userinfo, eosio::same_payer, [&](auto& st) {
        st.lock_state = lock_state;
    });
}


//钱包内部账户互相转账
ACTION ShadowUser::stationin(name from,name to,asset quantity,uint64_t timetamp,uint64_t nonce,string AuthSign,string memo,name executor){
    Identification_status(executor);
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid symbol name");
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    allowblances allowblance_table(_self, _self.value);
    auto allowblance = allowblance_table.find(quantity.symbol.code().raw());
    eosio_assert(allowblance != allowblance_table.end(), "This quantity is not defined");
    eosio_assert(allowblance->balance_switch, "This quantity switch is not open");
    eosio_assert(quantity.symbol == allowblance->balance.symbol, "symbol precision mismatch");

    userinfos userinfo_table(_self, from.value);
    auto userinfo = userinfo_table.begin();
    eosio_assert(userinfo != userinfo_table.end(), "from account is not define");
    eosio_assert(userinfo->lock_state, "from account is lock");

    userinfos userinfo_table2(_self, to.value);
    auto userinfo2 = userinfo_table2.begin();
    eosio_assert(userinfo2 != userinfo_table2.end(), "to account is not define");
    eosio_assert(userinfo2->lock_state, "to account is lock");

    userbalances userbalance_table(_self, from.value);
    auto userbalance = userbalance_table.find(quantity.symbol.code().raw());
    eosio_assert(userbalance != userbalance_table.end(), "from account is not find this quantity symbol");
    eosio_assert(userbalance->user_balance.amount >= quantity.amount,"Insufficient account balance");
    // eosio_assert(now() - timetamp <= 120,"This transfer is overtime");

    // for(int i=0;i<userinfo->timetamp.size();i++){
    //     if(now() - userinfo->timetamp.at(i) > 120){
    //         userinfo_table.modify(userinfo, eosio::same_payer, [&](auto& st) {
    //             st.timetamp.erase(st.timetamp.begin() + i);
    //             st.nonce.erase(st.nonce.begin() + i);
    //             i = i - 1;
    //         });
    //     }
    // }

    // for(int i=0;i<userinfo->nonce.size();i++){
    //     if(userinfo->nonce.at(i) == nonce){
    //         eosio_assert(0,"Replay attack");
    //     }
    // }

    // userinfo_table.modify(userinfo, eosio::same_payer, [&](auto& st) {
    //     st.timetamp.push_back(timetamp);
    //     st.nonce.push_back(nonce);
    // });

    auto message = from.to_string() + to.to_string() + quantity.to_string() + std::to_string(timetamp) + std::to_string(nonce)+memo;
    //Check_Sign(userinfo->public_key,message,AuthSign);
    userbalance_table.modify(userbalance, eosio::same_payer, [&](auto& st) {
        st.user_balance.amount -= quantity.amount;
    });

    userbalances userbalance_table2(_self, to.value);
    auto userbalance2 = userbalance_table2.find(quantity.symbol.code().raw());
    if(userbalance2 == userbalance_table2.end()){
        userbalance_table2.emplace(_self,[&](auto &st){
            st.token_contract = allowblance->token_contract;
            st.user_balance = quantity;
        });
    }
    else{
        userbalance_table2.modify(userbalance2, eosio::same_payer, [&](auto& st) {
            st.user_balance.amount += quantity.amount;
        });
    }
}

//钱包内部账户向外部账户转账
ACTION ShadowUser::stationout(name from,name to,asset quantity,uint64_t timetamp,uint64_t nonce,string AuthSign,string memo,name executor){
    Identification_status(executor);
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid symbol name");
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0

    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    allowblances allowblance_table(_self, _self.value);
    auto allowblance = allowblance_table.find(quantity.symbol.code().raw());
    eosio_assert(allowblance != allowblance_table.end(), "This quantity is not defined");
    eosio_assert(allowblance->balance_switch, "This quantity switch is not open");
    eosio_assert(quantity.symbol == allowblance->balance.symbol, "symbol precision mismatch");

    userinfos userinfo_table(_self, from.value);
    auto userinfo = userinfo_table.begin();
    eosio_assert(userinfo != userinfo_table.end(), "from account is not define");
    eosio_assert(userinfo->lock_state, "from account is lock");

    userbalances userbalance_table(_self, from.value);
    auto userbalance = userbalance_table.find(quantity.symbol.code().raw());
    eosio_assert(userbalance != userbalance_table.end(), "from account is not find this quantity symbol");
    eosio_assert(userbalance->user_balance.amount >= quantity.amount,"Insufficient account balance");
    // eosio_assert(now() - timetamp <= 120,"This transfer is overtime");

    // for(int i=0;i<userinfo->timetamp.size();i++){
    //     if(now() - userinfo->timetamp.at(i) > 120){
    //         userinfo_table.modify(userinfo, eosio::same_payer, [&](auto& st) {
    //             st.timetamp.erase(st.timetamp.begin() + i);
    //             st.nonce.erase(st.nonce.begin() + i);
    //             i = i - 1;
    //         });
    //     }
    // }

    // for(int i=0;i<userinfo->nonce.size();i++){
    //     if(userinfo->nonce.at(i) == nonce){
    //         eosio_assert(0,"Replay attack");
    //     }
    // }

    // userinfo_table.modify(userinfo, eosio::same_payer, [&](auto& st) {
    //     st.timetamp.push_back(timetamp);
    //     st.nonce.push_back(nonce);
    // });

    
    auto message = from.to_string() + to.to_string() + quantity.to_string() + std::to_string(timetamp) + std::to_string(nonce)+memo;
    //Check_Sign(userinfo->public_key,message,AuthSign);
    userbalance_table.modify(userbalance, eosio::same_payer, [&](auto& st) {
        st.user_balance.amount -= quantity.amount;
    });

    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {allowblance->token_contract, _self, to,quantity , memo});
}

void ShadowUser::Check_Sign(string Public,string message,string AuthSign){
    capi_checksum256 hash_message{};
    sha256(message.c_str(), message.size(), &hash_message);
    uint8_t sig[64] = {0};
    uint8_t Public_key[64] = {0};
    StrToHex(AuthSign.c_str(), sig, AuthSign.length()/2);
    StrToHex(Public.c_str(), Public_key, Public.length()/2);
    const struct uECC_Curve_t * curves = uECC_secp256k1();
    eosio_assert(uECC_verify(Public_key,hash_message.hash, 32, sig, curves),"uECC_verify() failed");
}


//外部账户向内部账户转账
void ShadowUser::ExternalTransfer(name from,asset quantity,name code,string memo){
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid symbol name");
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0

    //auto parts = split(memo, "|");
    //eosio_assert(parts.size() >= 2, "bad path format");
    //auto username = parts[0];
    //auto message = parts[1];
    auto username = memo;

    //************************ 新加接口 **************************
    inputlists inputlists_table(_self, _self.value);
    auto inputlist = inputlists_table.find(from.value);
    eosio_assert(inputlist != inputlists_table.end(), "This inputlist is not define");
    //***********************************************************

    allowblances allowblance_table(_self, _self.value);
    auto allowblance = allowblance_table.find(quantity.symbol.code().raw());
    eosio_assert(allowblance != allowblance_table.end(), "This quantity is not defined");
    eosio_assert(allowblance->token_contract == code,"This quantity contract is error");
    eosio_assert(quantity.symbol == allowblance->balance.symbol, "symbol precision mismatch");

    userinfos userinfo_table(_self, name(username).value);
    auto userinfo = userinfo_table.begin();
    eosio_assert(userinfo != userinfo_table.end(), "This username is not define");

    userbalances userbalance_table(_self, name(username).value);
    auto userbalance = userbalance_table.find(quantity.symbol.code().raw());
    if(userbalance == userbalance_table.end()){
        userbalance_table.emplace(_self,[&](auto &st){
            st.token_contract = code;
            st.user_balance = quantity;
        });
    }
    else{
        userbalance_table.modify(userbalance, eosio::same_payer, [&](auto& st) {
            st.user_balance.amount += quantity.amount;
        });
    }
}

//添加接口执行账户
ACTION ShadowUser::addwhitelist(name whitename){
    require_auth(_self);
    whitelists whitelist_table(_self, _self.value);
    auto whitelist = whitelist_table.find(whitename.value);
    eosio_assert(whitelist == whitelist_table.end(), "whitelist already defined");

    whitelist_table.emplace(_self, [&](auto& st) {
        st.whitename  = whitename;
    });
}

asset ShadowUser::get_balance(name contract, name owner, symbol_code sym) {
    accounts accountstable(contract, owner.value);
    const auto& ac = accountstable.get(sym.raw());
    return ac.balance;
}

asset ShadowUser::get_supply(name contract, symbol_code sym){
    stats statstable(contract, sym.raw());
    const auto& st = statstable.get(sym.raw());
    return st.supply;
}


void ShadowUser::transfer(name from, name to, asset quantity, string memo){
    if (from == _self) {
        // TODO: prevent withdrawal of funds
        return;
    }

    if (to != _self) 
        return;

    auto splitlist = split(memo,"&");
    auto splitlist2 = split(memo,",");
    if(splitlist.size() >= 2){
        if(splitlist[0] == "ExternalTransfer"){
            auto splitlist_memo = split(splitlist[1],"|");
            if(splitlist_memo.size() >= 2){
                ExternalTransfer(from,quantity,_code,splitlist_memo[0]);
                return;
            }
        }
    }

    if(splitlist2.size() >= 3){
        auto splitsend = split(splitlist2[0], ":");
        auto splitrc = split(splitlist2[1], ":");
        auto splitsym = split(splitlist2[2],":");
        if(splitsend[0] == "send" && splitrc[0] == "recieve" && splitsym[0] == "symbol"){
             auto splitrclist =  split(splitrc[1],"|");
             if(splitrclist.size() >=2){
                 ExternalTransfer(from,quantity,_code,splitrclist[1]);
                 return;
             }
        }    
    }
}

ACTION ShadowUser::inlinetransf(name contract, name from, name to, asset quantity, string memo){
    require_auth(_self);

    //EMIT_CONVERSION_EVENT("action:", contract.to_string(),"-----------------------");

    action(
        permission_level{ _self, name("active") },
        contract, name("transfer"),
        std::make_tuple(from, to, quantity, memo)
    ).send();
}

void ShadowUser::StrToHex(const char lpSrcStr[], unsigned char lpRetBytes[], size_t lpRetSize)
{
    if (lpSrcStr != NULL && lpRetBytes != NULL && lpRetSize > 0)
    {
        size_t uiLength = strlen(lpSrcStr);
        if (uiLength % 2 == 0)
        {
            size_t i = 0;
            size_t n = 0;
            while (*lpSrcStr != 0 && (n = ((i++) >> 1)) < lpRetSize)
            {
                lpRetBytes[n] <<= 4;
                if (*lpSrcStr >= '0' && *lpSrcStr <= '9')
                {
                    lpRetBytes[n] |= *lpSrcStr - '0';
                }
                else if (*lpSrcStr >= 'a' && *lpSrcStr <= 'f')
                {
                    lpRetBytes[n] |= *lpSrcStr - 'a' + 10;
                }
                else if (*lpSrcStr >= 'A' && *lpSrcStr <= 'F')
                {
                    lpRetBytes[n] |= *lpSrcStr - 'A' + 10;
                }
                lpSrcStr++;
            }
            lpRetSize = n;
        }
    }
}


extern "C" {
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (action == "transfer"_n.value && code != receiver) {
            eosio::execute_action(eosio::name(receiver), eosio::name(code), &ShadowUser::transfer);
        }
        if (code == receiver) {
            switch (action) { 
                EOSIO_DISPATCH_HELPER(ShadowUser, (createuser)(addbalance)(rstpubkey)(rstuserlock)(stationin)(stationout)(initswitch)(resetswitch)
                (rstblswitch)(addwhitelist)(inlinetransf)(addinputlist)) 
            }    
        }
        eosio_exit(0);
    }
}