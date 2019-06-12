#include "./BancorConverter.hpp"
#include "../Common/common.hpp"
#include "../JsonFile/lib/cJSON.c"
#include <math.h>

using namespace eosio;
using std::string;
#define Basics_Symbol "EOS"
#define Basics_Symbol_Contract "tokmemodates"

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


//添加内部可转换代币参数
ACTION BancorConverter::addcurrency(name contract,asset currency,name saveaccount,name proaccount,uint64_t max_fee,uint64_t buy_fee,uint64_t buy_baseline,
                            uint64_t sell_fee,uint64_t sell_baseline,uint64_t save_fee,uint64_t opentime,uint64_t closetime,bool enabled,name executor){
    Identification_status(executor);

    //判断输入的用户名是否为12位
    eosio_assert(contract.length() == 12, "contract is error");
    eosio_assert(saveaccount.length() == 12, "saveaccount is error");
    eosio_assert(proaccount.length() == 12, "proaccount is error");

    //判断代币参数是否合法
    eosio_assert(currency.is_valid(), "invalid currency");   //判断支付货币是否可用
    eosio_assert(currency.symbol.is_valid(), "invalid symbol name");
    eosio_assert(currency.symbol.precision() == 4, "currency symbol precision mismatch");    //支付货币应该 大于0

    //判断手续费是否合法
    eosio_assert(max_fee <= 1000, "maximum fee must be lower or equal to 1000"); //最高的费用
    eosio_assert(buy_fee <= max_fee, "fee must be lower or equal to the maximum fee");
    eosio_assert(buy_fee >= buy_baseline, "buy_fee less than buy_baseline");
    eosio_assert(sell_fee <= max_fee, "fee must be lower or equal to the maximum fee");
    eosio_assert(sell_fee >= sell_baseline, "sell_fee less than sell_baseline");
    eosio_assert(save_fee <= max_fee, "fee must be lower or equal to the maximum fee");

    //开始时间和结束时间必须相差一天的时间
    eosio_assert(closetime - opentime >= 86400, "closetime is error");
     
    //记录总表
    currencylsts currency_table(_self, _self.value);
    auto existing = currency_table.find(currency.symbol.code().raw());
    eosio_assert(existing == currency_table.end(), "This currency already defined");      //判断支付货币是否可用
    currency_table.emplace(_self, [&](auto& st) {
        st.contract  = contract;
        st.currency  = currency;
        st.saveaccount  = saveaccount;
        st.proaccount   = proaccount;
        st.max_fee = max_fee;
        st.buy_fee = buy_fee;
        st.buy_baseline = buy_baseline;
        st.sell_fee = sell_fee;
        st.sell_baseline = sell_baseline;
        st.save_fee = save_fee;
        st.enabled  = enabled;
        st.opentime = opentime;
        st.closetime = closetime;
    });
}

//初始化总开关
ACTION BancorConverter::initswitch(bool switch_lock,name executor){
    Identification_status(executor);
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(!switch_exists, "switchs already defined");

    switch_t new_switch;
    new_switch.switch_lock  = switch_lock;
    switch_table.set(new_switch, _self);
}

//添加内部基准代币
ACTION BancorConverter::addreserve(asset tokentype,name contract,asset currency,uint64_t ratio,name executor){
    Identification_status(executor);

    //判断代币索引参数是否合法
    eosio_assert(tokentype.is_valid(), "invalid tokentype");
    eosio_assert(tokentype.symbol.is_valid(), "invalid tokentype symbol name");
    eosio_assert(tokentype.symbol.precision() == 4, "tokentype symbol precision mismatch");

    //判断代币参数是否合法
    eosio_assert(currency.is_valid(), "invalid currency");      //判断支付货币是否可用
    eosio_assert(currency.symbol.is_valid(), "invalid currency symbol name");
    eosio_assert(currency.amount > 0, "currency.amount is error");
    eosio_assert(currency.symbol.precision() == 4, "currency symbol precision mismatch"); 

    eosio_assert(ratio > 0 && ratio <= 1000, "ratio must be between 1 and 1000"); //判断cw值是否合法
    eosio_assert(currency.symbol.code().raw() == symbol_code(Basics_Symbol).raw(),"currency is symbol is error");
    eosio_assert(contract == name(Basics_Symbol_Contract),"contract is error");   //判断contract　是否为预先设定好的token发布合约

    reserves reserves_table(_self, _self.value);
    auto existing = reserves_table.find(tokentype.symbol.code().raw());
    eosio_assert(existing == reserves_table.end(), "existing already defined");
    reserves_table.emplace(_self, [&](auto& s) {
        s.tokentype   = tokentype;
        s.contract    = contract;
        s.currency    = currency;
        s.ratio       = ratio;
    });

    curreserves curreserves_table(_self, _self.value);
    auto curreserve = curreserves_table.find(tokentype.symbol.code().raw());  
    eosio_assert(curreserve == curreserves_table.end(), "curreserve already defined");     
    curreserves_table.emplace(_self, [&](auto& st) {
        st.tokentype = tokentype;
        st.currency = asset(0,currency.symbol);
    });
}


//修改某代币的买卖手续费
ACTION BancorConverter::resetfee(string symbol,uint64_t buy_fee,uint64_t sell_fee,name executor){
    Identification_status(executor);
    currencylsts currency_table(_self, _self.value);
    auto existing = currency_table.find(symbol_code(symbol).raw());
    eosio_assert(existing != currency_table.end(), "This currency is not defined");      

    auto max_fee = existing->max_fee;
    eosio_assert(buy_fee <= max_fee, "fee must be lower or equal to the maximum fee");
    eosio_assert(sell_fee <= max_fee, "fee must be lower or equal to the maximum fee");
    eosio_assert(buy_fee >= existing->buy_baseline, "buy_fee less than buy_baseline");
    eosio_assert(sell_fee >= existing->sell_baseline, "sell_fee less than sell_baseline");

    currency_table.modify(existing, eosio::same_payer, [&](auto& st) {
        st.buy_fee = buy_fee;
        st.sell_fee = sell_fee;
    });
}

//修改某代币的买卖手续费底费
ACTION BancorConverter::resetline(string symbol,uint64_t buy_baseline,uint64_t sell_baseline,name executor){
    Identification_status(executor);
    currencylsts currency_table(_self, _self.value);
    auto existing = currency_table.find(symbol_code(symbol).raw());
    eosio_assert(existing != currency_table.end(), "This currency is not defined");  

    eosio_assert(existing->buy_fee >= buy_baseline, "buy_fee less than buy_baseline");
    eosio_assert(existing->sell_fee >= sell_baseline, "sell_fee less than sell_baseline");

    currency_table.modify(existing, eosio::same_payer, [&](auto& st) {
        st.buy_baseline = buy_baseline;
        st.sell_baseline = sell_baseline;
    });
}


//修改开发方手续费
ACTION BancorConverter::resetsavfee(string symbol,uint64_t save_fee,name executor){
    Identification_status(executor);
    currencylsts currency_table(_self, _self.value);
    auto existing = currency_table.find(symbol_code(symbol).raw());
    eosio_assert(existing != currency_table.end(), "This currency is not defined");  

    auto max_fee = existing->max_fee;
    eosio_assert(save_fee <= max_fee, "fee must be lower or equal to the maximum fee");

    currency_table.modify(existing, eosio::same_payer, [&](auto& st) {
        st.save_fee = save_fee;
    });
}

//校验接口执行者
void BancorConverter::Identification_status(name executor){
    require_auth(executor);
    whitelists whitelist_table(_self, _self.value);
    auto whitelist = whitelist_table.find(executor.value);
    eosio_assert(whitelist != whitelist_table.end(), "whitelist is not define");
}

//转换兑换
void BancorConverter::convert(name from, asset quantity,string shadow_recieve,string exchange_symbol,name code){    
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity symbol name");
    eosio_assert(quantity.symbol.precision() == 4, "quantity symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0
    auto from_amount = quantity.amount / pow(10, quantity.symbol.precision());  //取出小数点 获取真实值

    uint64_t symbolraw = 0;
    if(exchange_symbol == Basics_Symbol){
        symbolraw = quantity.symbol.code().raw();
    }
    else{
        symbolraw = symbol_code(exchange_symbol).raw();
    }

    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    currencylsts currency_table(_self, _self.value);
    auto cur_existing = currency_table.find(symbolraw);
    eosio_assert(cur_existing != currency_table.end(), "This currency is not defined");
    eosio_assert(now() >= cur_existing->opentime, "Open time not yet arrived");
    eosio_assert(cur_existing->enabled, "converter is disabled"); 

    //eosio_assert(cur_existing->network == from, "converter can only receive from network contract");
    //eosio_assert(recieve_name == from.to_string(), "splitrc error");
    //auto contract_name = name(send_name);
    //eosio_assert(contract_name == _self, "wrong converter");  //判断是否是本合约

    auto from_path_currency = quantity.symbol.code().raw(); //获取出from 所支付的货币类型索引
    auto to_path_currency = symbol_code(exchange_symbol).raw(); //获取出所要换取货币的类型索引
    eosio_assert(from_path_currency != to_path_currency, "cannot convert to self");  //如何支付的货币和所要换取的货币是一样的则报错退出
    auto from_token = get_reserve(from_path_currency,to_path_currency);
    auto to_token = get_reserve(to_path_currency,from_path_currency);

    auto from_currency = from_token.currency; //获取from 的存储金额
    auto to_currency = to_token.currency;   //获取to 的存储金额

    auto from_contract = from_token.contract;  //获取from发币的合约
    auto to_contract = to_token.contract;   //获取to发币的合约
    
    bool incoming_smart_token = (from_currency.symbol.code().raw() != symbol_code(Basics_Symbol).raw()); //判断是否用中转币兑换
    bool outgoing_smart_token = (to_currency.symbol.code().raw() != symbol_code(Basics_Symbol).raw());     //判断是要换中转币
    eosio_assert(quantity.symbol.code().raw() == symbol_code(Basics_Symbol).raw() || symbol_code(exchange_symbol).raw() == symbol_code(Basics_Symbol).raw(), "cannot convert"); //制约 两种发行代币互相转换

    eosio_assert(code == from_contract, "unknown 'from' contract");
    double to_tokens = 0;
    int64_t to_amount = 0;
    auto issue = false;
    double balance = 0;

    uint16_t benchmark_day = ((cur_existing->closetime )-(cur_existing->opentime))*1.0 / (3600 * 1.0) / (24 * 1.0); 
    uint16_t new_day = (now() - (cur_existing->opentime))*1.0 / (3600 * 1.0) / (24 * 1.0); 
    if(incoming_smart_token){
        curreserves curreserves_table(_self, _self.value);
        auto existing = curreserves_table.find(from_currency.symbol.code().raw());
        eosio_assert(existing != curreserves_table.end(), "curreserves_table is not init");
        
        auto current_smart_supply = ((get_supply(from_token.contract, from_token.currency.symbol.code())).amount + from_token.currency.amount) / pow(10, from_token.currency.symbol.precision());
        //balance = ((get_balance(to_contract, _self, to_currency.symbol.code())).amount + to_currency.amount) / pow(10, to_currency.symbol.precision()); //获取本合约 还剩多少货币to的代币
        balance = (existing->currency.amount + to_currency.amount) / pow(10, to_currency.symbol.precision());
        auto ratio = to_token.ratio * 1.0;     //获取EOS的比率  

        auto nowfree = cur_existing->sell_fee - (cur_existing->sell_fee - cur_existing->sell_baseline)/(benchmark_day * 1.0) * new_day;
        to_tokens = balance *(1-pow(1-from_amount/current_smart_supply,1/(ratio/1000.0)));
        if(nowfree < cur_existing->sell_baseline){
            nowfree = cur_existing->sell_baseline * 1.0;
        }

        if(nowfree > 0 && cur_existing->save_fee > 0){
            double profree = (1.0 * nowfree / 1000.0);
            auto profree_number = to_tokens * profree;
            double savefree = (1.0 * cur_existing->save_fee /1000.0);
            auto savfree_number = to_tokens * savefree;

            string pro_JsonMemo = "";
            string forward_JsonMemo = "";
            pro_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);
            forward_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);

            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {to_contract, _self, cur_existing->proaccount, asset(profree_number*pow(10, to_currency.symbol.precision()),to_currency.symbol), pro_JsonMemo});
            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {to_contract, _self, cur_existing->saveaccount, asset(savfree_number*pow(10, to_currency.symbol.precision()),to_currency.symbol), forward_JsonMemo});
            to_amount = ((to_tokens - profree_number - savfree_number) * pow(10, to_currency.symbol.precision()));         
        }
        else if(nowfree > 0 && cur_existing->save_fee == 0){
            double profree = (1.0 * nowfree / 1000.0);
            auto profree_number = to_tokens * profree;

            string pro_JsonMemo = "";
            pro_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);

            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {to_contract, _self, cur_existing->proaccount, asset(profree_number*pow(10, to_currency.symbol.precision()),to_currency.symbol), pro_JsonMemo});
            to_amount = ((to_tokens - profree_number) * pow(10, to_currency.symbol.precision()));  
        }
        else if(nowfree == 0 && cur_existing->save_fee > 0){
            double savefree = (1.0 * cur_existing->save_fee /1000.0);
            auto savfree_number = to_tokens * savefree;

            string forward_JsonMemo = "";
            forward_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);

            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {to_contract, _self, cur_existing->saveaccount, asset(savfree_number*pow(10, to_currency.symbol.precision()),to_currency.symbol), forward_JsonMemo});
            to_amount = ((to_tokens - savfree_number) * pow(10, to_currency.symbol.precision()));  
        }
        else{
            to_amount = (to_tokens * pow(10, to_currency.symbol.precision()));
        }

        eosio_assert(existing->currency.amount >= to_amount, "This balance is Null");
        curreserves_table.modify(existing, eosio::same_payer, [&](auto& st) {
            st.currency.amount =  st.currency.amount - to_amount;
        });
    
        action(
            permission_level{ _self, "active"_n },
            from_token.contract, "retire"_n,
            std::make_tuple(quantity, std::string("destroy on conversion"))
        ).send();
    }
    else if(outgoing_smart_token){
        
        curreserves curreserves_table(_self, _self.value);
        auto existing = curreserves_table.find(to_currency.symbol.code().raw());
        eosio_assert(existing != curreserves_table.end(), "curreserves_table is not init");

        auto current_smart_supply = ((get_supply(to_token.contract, to_token.currency.symbol.code())).amount + to_token.currency.amount) / pow(10, to_token.currency.symbol.precision());
        //balance = ((get_balance(from_contract, _self, from_currency.symbol.code())).amount + from_currency.amount - quantity.amount) / pow(10, from_currency.symbol.precision()); //获取本合约 还剩多少货币to的代币
        balance = (existing->currency.amount + from_currency.amount) / pow(10, from_currency.symbol.precision());
        auto ratio = from_token.ratio *1.0;     //获取EOS的比率  

        auto nowfree = cur_existing->buy_fee - (cur_existing->buy_fee - cur_existing->buy_baseline)/(benchmark_day * 1.0) * new_day;
        if(nowfree < cur_existing->sell_baseline){
            nowfree = cur_existing->sell_baseline * 1.0;
        }

        if(nowfree > 0 && cur_existing->save_fee > 0){
            double profree = (1.0 * nowfree / 1000.0);
            auto profree_number = from_amount * profree;
            double savefree = (1.0 * cur_existing->save_fee /1000.0);
            auto savfree_number = from_amount * savefree;        
            from_amount = from_amount - profree_number - savfree_number;

            string pro_JsonMemo = "";
            string forward_JsonMemo = "";
            pro_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);
            forward_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);
       
            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {from_contract, _self, cur_existing->proaccount, asset(profree_number*pow(10, from_currency.symbol.precision()),from_currency.symbol), pro_JsonMemo});        
            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {from_contract, _self, cur_existing->saveaccount, asset(savfree_number*pow(10, from_currency.symbol.precision()),from_currency.symbol), forward_JsonMemo});      
        }
        else if(nowfree > 0 && cur_existing->save_fee == 0){
            double profree = (1.0 * nowfree / 1000.0);
            auto profree_number = from_amount * profree;
            from_amount = from_amount - profree_number;

            string pro_JsonMemo = "";
            pro_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);

            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {from_contract, _self, cur_existing->proaccount, asset(profree_number*pow(10, from_currency.symbol.precision()),from_currency.symbol), pro_JsonMemo});
        }
        else if(nowfree == 0 && cur_existing->save_fee > 0){
            double savefree = (1.0 * cur_existing->save_fee /1000.0);
            auto savfree_number = from_amount * savefree;
            from_amount = from_amount - savfree_number;

            string forward_JsonMemo = "";
            forward_JsonMemo = Json_Transformation("BanMortageReward","",exchange_symbol);

            SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {from_contract, _self, cur_existing->saveaccount, asset(savfree_number*pow(10, from_currency.symbol.precision()),from_currency.symbol), forward_JsonMemo}); 
        }
     
        to_tokens = current_smart_supply*(pow(1+from_amount/balance,ratio/1000.0)-1);
        to_amount = (to_tokens * pow(10, to_token.currency.symbol.precision()));

        issue = true;
     
        curreserves_table.modify(existing, eosio::same_payer, [&](auto& st) {
            st.currency.amount =  st.currency.amount + from_amount * pow(10, from_currency.symbol.precision());
        });
    }
    
    string newmemo = "";
    newmemo = Json_Transformation("convert",shadow_recieve,exchange_symbol);
    auto new_asset = asset(to_amount, to_currency.symbol);
    
    if (issue)
    {    
        action(
            permission_level{ _self, "active"_n },
            to_contract, "issue"_n,
            std::make_tuple(from, new_asset, newmemo) 
        ).send();
    }    
    else{
        action(
            permission_level{ _self, "active"_n },
            to_contract, "transfer"_n,
            std::make_tuple(_self, from, new_asset, newmemo)
        ).send();
    }
}


//修改某代币开关
ACTION BancorConverter::switchbc(string symbol,bool enabled,name executor){
    Identification_status(executor);
    currencylsts currency_table(_self, _self.value);
    auto existing = currency_table.find(symbol_code(symbol).raw());
    eosio_assert(existing != currency_table.end(), "This currency is not defined");  

    currency_table.modify(existing, eosio::same_payer, [&](auto& st) {
        st.enabled = enabled;
    });
}

//重置总开关
ACTION BancorConverter::resetswitch(bool switch_lock,name executor){
    Identification_status(executor);
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switchs is not define");

    auto switchs = switch_table.get();
    switchs.switch_lock = switch_lock;
    switch_table.set(switchs, _self);
}

//修改某代币开始结束时间
ACTION BancorConverter::resettime(string symbol,uint64_t opentime,uint64_t closetime,name executor){
    Identification_status(executor);
    currencylsts currency_table(_self, _self.value);
    auto existing = currency_table.find(symbol_code(symbol).raw());
    eosio_assert(existing != currency_table.end(), "This currency is not defined");  
    eosio_assert(closetime - opentime >= 86400, "closetime is error");

    currency_table.modify(existing, eosio::same_payer, [&](auto& st) {
        st.opentime = opentime;
        st.closetime = closetime;
    });    
}

//添加接口执行者
ACTION BancorConverter::addwhitelist(name whitename){
    require_auth(_self);

    //判断输入的用户名是否为12位
    eosio_assert(whitename.length() == 12, "whitename is error");

    whitelists whitelist_table(_self, _self.value);
    auto whitelist = whitelist_table.find(whitename.value);
    eosio_assert(whitelist == whitelist_table.end(), "whitelist already defined");

    whitelist_table.emplace(_self, [&](auto& st) {
        st.whitename  = whitename;
    });
}

const BancorConverter::reserve_t& BancorConverter::get_reserve(uint64_t fromname,uint64_t toname) {
    if(fromname != symbol_code(Basics_Symbol).raw()){
        currencylsts currency_table(_self, _self.value);
        auto nowcurrency = currency_table.find(fromname);
        eosio_assert(nowcurrency != currency_table.end(), "This currency is not found");
        static reserve_t temp_reserve;
        temp_reserve.ratio = 0;
        temp_reserve.tokentype = nowcurrency->currency;
        temp_reserve.contract = nowcurrency->contract;
        temp_reserve.currency = nowcurrency->currency;
        return temp_reserve;  
    }

    reserves reserves_table(_self, _self.value);
    auto existing = reserves_table.find(toname);
    eosio_assert(existing != reserves_table.end(), "reserve not found");
    return *existing;   
}

void BancorConverter::recharge(asset quantity,string exchange_symbol,name code){
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity symbol name");
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0
    eosio_assert(quantity.symbol.precision() == 4, "quantity symbol precision mismatch"); 

    reserves reserves_table(_self, _self.value);
    auto reserve = reserves_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(reserve != reserves_table.end(), "reserve not found");
    eosio_assert(quantity.symbol == reserve->currency.symbol, "quantity symbol is error"); 
    eosio_assert(reserve->contract == code, "unknown 'from' contract");

    curreserves curreserves_table(_self, _self.value);
    auto curreserve = curreserves_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(curreserve != curreserves_table.end(), "curreserve not found");

    curreserves_table.modify(curreserve, eosio::same_payer, [&](auto& st) {
        st.currency.amount =  st.currency.amount + quantity.amount;
    });

    if(reserve->currency.amount - quantity.amount >=0){
        reserves_table.modify(reserve, eosio::same_payer, [&](auto& st) {
            st.currency.amount =  st.currency.amount - quantity.amount;
        }); 
    }
    else{
        reserves_table.modify(reserve, eosio::same_payer, [&](auto& st) {
            st.currency.amount =  0;
        }); 
    }  
}

asset BancorConverter::get_balance(name contract, name owner, symbol_code sym) {
    accounts accountstable(contract, owner.value);
    const auto& ac = accountstable.get(sym.raw());
    return ac.balance;
}

asset BancorConverter::get_supply(name contract, symbol_code sym){
    stats statstable(contract, sym.raw());
    const auto& st = statstable.get(sym.raw());
    return st.supply;
}

void BancorConverter::transfer(name from, name to, asset quantity, string memo){
    if (from == _self) {
        // TODO: prevent withdrawal of funds
        return;
    }

    if (to != _self) 
        return;

    //新添加Json解析字符串****************************************************
    cJSON *json=0,*json_type=0,*json_symbol=0,*json_shadow_rec=0;
	json = cJSON_Parse(memo.c_str());
    json_type = cJSON_GetObjectItem(json, "type");
    json_symbol = cJSON_GetObjectItem(json, "symbol");
    json_shadow_rec = cJSON_GetObjectItem(json, "shadow_recieve");

    if(!json || !json_symbol || !json_type){
        return;  
    }
    if(json_symbol->type != cJSON_String || json_type->type != cJSON_String){
        return;
    }

    string json_type_str = json_type->valuestring;
    string json_symbol_str = json_symbol->valuestring;
    string shadow_recieve = "";
    if(json_shadow_rec){
        if(json_shadow_rec->type != cJSON_String)return; 
        shadow_recieve = json_shadow_rec->valuestring;
    }

    cJSON_Delete(json);	
    
    if( json_type_str == "convert"){
        convert(from,quantity,shadow_recieve,json_symbol_str,_code);
    }
    else if(json_type_str == "TgeRecharge"){
        recharge(quantity,json_symbol_str,_code);
    }
    //********************************************************************** 
}

string BancorConverter::Json_Transformation(string type,string shadow_recieve,string symbol){
    string memo = "{";
    memo += "\"type\":\"" + type + "\"";
    memo += ",";
    if(shadow_recieve != ""){
        memo += "\"shadow_recieve\":\"" + shadow_recieve + "\"";
        memo += ",";
    } 
    
    memo += "\"symbol\":\"" + symbol + "\"";
    memo +="}";
    return memo;
}

ACTION BancorConverter::inlinetransf(name contract, name from, name to, asset quantity, string memo){
    require_auth(_self);
    action(
        permission_level{ from, name("active") },
        contract, name("transfer"),
        std::make_tuple(from, to, quantity, memo)
    ).send();
}

extern "C" {
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (action == "transfer"_n.value && code != receiver) {
            eosio::execute_action(eosio::name(receiver), eosio::name(code), &BancorConverter::transfer);
        }
        if (code == receiver) {
            switch (action) { 
                EOSIO_DISPATCH_HELPER(BancorConverter, (addreserve)(switchbc)(resetfee)(resetsavfee)(inlinetransf)(resettime)(addwhitelist)
                (resetline)(addcurrency)(initswitch)(resetswitch)) 
            }    
        }
        eosio_exit(0);
    }
}


