#include "./ExChange.hpp"
#include "../JsonFile/lib/cJSON.c"
#include <math.h>

using namespace eosio;
#define Basics_Symbol "EOS"
#define Basics_Symbol_Contract "eosio.token"

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

//添加新代币
ACTION ExChange::addassets(name token_contract,name ban_account,asset user_currency,name pro_account,asset pro_currency,name forward_account,asset forward_currency,
    uint64_t min_quantity,uint64_t exchangerd,uint64_t open_buytime,uint64_t open_extracttime,bool buy_switch,bool extract_switch,name executor){

    Identification_status(executor);
    
    //判断输入的用户名是否为12位
    eosio_assert(token_contract.length() == 12, "token_contract is error");
    eosio_assert(ban_account.length() == 12, "ban_account is error");
    eosio_assert(pro_account.length() == 12, "pro_account is error");
    eosio_assert(forward_account.length() == 12, "forward_account is error");

    //判断tge可供用户购买的代币参数是否合法
    eosio_assert(user_currency.is_valid(), "invalid user_currency");      //判断支付货币是否可用
    eosio_assert(user_currency.symbol.is_valid(), "invalid user_currency symbol name");
    eosio_assert(user_currency.symbol.precision() == 4, "user_currency symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(user_currency.amount > 0, "zero user_currency amount is disallowed"); //支付货币应该 大于0
   
    //判断给项目方的tge代币参数是否合法
    eosio_assert(pro_currency.is_valid(), "invalid pro_currency");      //判断支付货币是否可用
    eosio_assert(pro_currency.symbol.is_valid(), "invalid pro_currency symbol name");
    eosio_assert(pro_currency.symbol.precision() == 4, "pro_currency symbol precision mismatch"); //判断代币必须为小数点４位
    eosio_assert(pro_currency.amount > 0, "zero pro_currency amount is disallowed"); //支付货币应该 大于0

    //判断给开发方tge代币参数是否合法
    eosio_assert(forward_currency.is_valid(), "invalid forward_currency");      //判断支付货币是否可用
    eosio_assert(forward_currency.symbol.is_valid(), "invalid forward_currency symbol name");
    eosio_assert(forward_currency.symbol.precision() == 4, "forward_currency symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(forward_currency.amount > 0, "zero forward_currency amount is disallowed"); //支付货币应该 大于0

    //判断上述三个账户代币的类型是否相等
    eosio_assert(user_currency.symbol == pro_currency.symbol && user_currency.symbol == forward_currency.symbol, "currency symbol Unequal"); 

    //判断代币是否已经发放
    auto current_balance = get_balance(token_contract,_self,user_currency.symbol.code());
    eosio_assert(current_balance.amount >= user_currency.amount + pro_currency.amount
    + forward_currency.amount , "current_balance is error"); 

    //开始购买时间和开始提取时间必须相差一天的时间
    eosio_assert(open_extracttime - open_buytime >= 86400, "closetime is error");

    //代币倍数要大于０这里的倍数是指　EOS 是该代币　exchangerd/1000倍
    eosio_assert(exchangerd > 0 , "exchangerd error");

    //记录总表
    assets assets_table(_self,_self.value);
    auto asset_info = assets_table.find(user_currency.symbol.code().raw());
    eosio_assert(asset_info == assets_table.end(), "assetinfo already defined");
    assets_table.emplace(_self, [&](auto& st){
        st.token_contract = token_contract;
        st.ban_account = ban_account;
        st.user_currency = user_currency;
        st.pro_account = pro_account;
        st.pro_currency = pro_currency;
        st.forward_account = forward_account;
        st.forward_currency = forward_currency;
        st.min_quantity = min_quantity;
        st.exchangerd = exchangerd;
        st.open_buytime = open_buytime;
        st.open_extracttime = open_extracttime;
        st.buy_switch = buy_switch;
        st.extract_switch = extract_switch;
    });

    //记录普通用户购买总额和剩余总额
    tgeusers tgeusers_table(_self,_self.value);
    auto tgeusers_info = tgeusers_table.find(user_currency.symbol.code().raw());
    eosio_assert(tgeusers_info == tgeusers_table.end(), "tgeusers_info already defined");
    tgeusers_table.emplace(_self, [&](auto& st){
        st.user_currency = user_currency;
        st.user_totbuy = user_currency;
    });

    //记录项目方户购买总额和剩余总额
    tgepros tgepros_table(_self,_self.value);
    auto tgepros_info = tgepros_table.find(pro_currency.symbol.code().raw());
    eosio_assert(tgepros_info == tgepros_table.end(), "tgepros_info already defined");
    tgepros_table.emplace(_self, [&](auto& st){
        st.pro_currency = pro_currency;
        st.pro_totbuy = pro_currency;
        st.time = 0;
    });

    //记录开发方购买总额和剩余总额
    tgeforwards tgeforwards_table(_self,_self.value);
    auto tgeforwards_info = tgeforwards_table.find(forward_currency.symbol.code().raw());
    eosio_assert(tgeforwards_info == tgeforwards_table.end(), "tgepros_info already defined");
    tgeforwards_table.emplace(_self, [&](auto& st){
        st.forward_currency = forward_currency;
        st.forward_totbuy = forward_currency;
        st.time = 0;
    });
}

//添加转换代币
ACTION ExChange::addvirtual(asset tokentype,name virtua_tokcontract,asset virtua_currency,name executor){
    Identification_status(executor);

    //判断转换代币创建账户的合法性
    eosio_assert(virtua_tokcontract.length() == 12, "virtua_tokcontract is error");

    //判断代币索引参数是否合法
    eosio_assert(tokentype.is_valid(), "invalid tokentype");      //判断支付货币是否可用
    eosio_assert(tokentype.symbol.is_valid(), "invalid tokentype symbol name");
    eosio_assert(tokentype.symbol.precision() == 4, "tokentype symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(tokentype.amount >= 0, "zero tokentype amount is disallowed"); //支付货币应该 大于0

    //判断转换代币参数是否合法
    eosio_assert(virtua_currency.is_valid(), "invalid user_currency");      //判断支付货币是否可用
    eosio_assert(virtua_currency.symbol.is_valid(), "invalid user_currency symbol name");
    eosio_assert(virtua_currency.symbol.precision() == 4, "user_currency symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(virtua_currency.amount > 0, "zero user_currency amount is disallowed"); //支付货币应该 大于0

    assets assets_table(_self,_self.value);
    auto asset_info = assets_table.find(tokentype.symbol.code().raw());
    eosio_assert(asset_info != assets_table.end(), "This tokentype is not defined");

    virtualtoks virtualtoks_table(_self,_self.value);
    auto virtualtoks_info = virtualtoks_table.find(virtua_currency.symbol.code().raw());
    eosio_assert(virtualtoks_info == virtualtoks_table.end(), "virtualsets_info virtua_currency already defined");
    virtualtoks_table.emplace(_self, [&](auto& st){
        st.virtua_currency = virtua_currency;
    });

    //判断转换代币是否已经发放
    auto virtual_balance = get_balance(virtua_tokcontract,_self,virtua_currency.symbol.code());
    eosio_assert(virtual_balance.amount >= asset_info->user_currency.amount, "virtual_balance is error");

    //添加转换代币
    virtualsets virtualsets_table(_self,_self.value);
    auto virtualsets_info = virtualsets_table.find(tokentype.symbol.code().raw());
    eosio_assert(virtualsets_info == virtualsets_table.end(), "virtualsets_info tokentype already defined");
    virtualsets_table.emplace(_self, [&](auto& st){
        st.tokentype = tokentype;
        st.virtua_tokcontract = virtua_tokcontract;
        st.virtua_currency = virtua_currency;
        st.virtua_totbuy = virtua_currency;
    });
}

//为新代币添加bancor计算
ACTION ExChange::addbancor(name token_contract,asset bancor_supply,asset bancor_balance,uint64_t bancor_radio,name executor){
    Identification_status(executor);

    //判断已supply发行总量参数是否合法    
    eosio_assert(bancor_supply.is_valid(), "invalid bancor_supply");      //判断支付货币是否可用
    eosio_assert(bancor_supply.symbol.is_valid(), "invalid bancor_supply symbol name");
    eosio_assert(bancor_supply.symbol.precision() == 4, "bancor_supply symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(bancor_supply.amount > 0, "zero bancor_supply amount is disallowed"); //支付货币应该 大于0

    //判断已balance参数是否合法
    eosio_assert(bancor_balance.is_valid(), "invalid bancor_balance");      //判断支付货币是否可用
    eosio_assert(bancor_balance.symbol.is_valid(), "invalid bancor_balance symbol name");
    eosio_assert(bancor_balance.symbol.precision() == 4, "bancor_balance symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(bancor_balance.amount > 0, "zero bancor_balance amount is disallowed"); //支付货币应该 大于0
    eosio_assert(bancor_balance.symbol.code().raw() == symbol_code(Basics_Symbol).raw(),"bancor_balance is symbol is error");
    eosio_assert(token_contract == name(Basics_Symbol_Contract),"token_contract is error"); 

    //判断输入的cw值是否大于０
    eosio_assert(bancor_radio > 0, "bancor_radio is error");

    bancorsets bancorsets_table(_self, _self.value);
    auto bancorsets_info = bancorsets_table.find(bancor_supply.symbol.code().raw());
    eosio_assert(bancorsets_info == bancorsets_table.end(), "bancorsets_info already defined");

    bancorsets_table.emplace(_self, [&](auto& st){
        st.token_contract = token_contract;
        st.bancor_supply = bancor_supply;
        st.bancor_balance = bancor_balance;
        st.bancor_radio = bancor_radio;
    });
}

//初始化总开关
ACTION ExChange::initswitch(bool switch_lock,name executor){
    Identification_status(executor);
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(!switch_exists, "switchs already defined");

    switch_t new_switch;
    new_switch.switch_lock  = switch_lock;
    switch_table.set(new_switch, _self);

}

//判断可执行权限
void ExChange::Identification_status(name executor){
    require_auth(executor);
    whitelists whitelist_table(_self, _self.value);
    auto whitelist = whitelist_table.find(executor.value);
    eosio_assert(whitelist != whitelist_table.end(), "whitelist is not define");
}

//设置总开关状态
ACTION ExChange::resetswitch(bool switch_lock,name executor){
    Identification_status(executor);
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switchs is not define");

    auto switchs = switch_table.get();
    switchs.switch_lock = switch_lock;
    switch_table.set(switchs, _self);
}

void ExChange::Transfer_Mortage(name from, asset quantity,string shadow_recieve,string exchange_symbol,name code){
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity symbol name");
    eosio_assert(quantity.symbol.precision() == 4, "quantity symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0

    //总开关
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    assets assets_table(_self,_self.value);
    auto assets_info = assets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(assets_info != assets_table.end(), "This exchange_symbol is not define");
    eosio_assert(assets_info->buy_switch, "This token buy_switch is not open");

    //只有在开放购买和未开放领取的时间段可以买
    eosio_assert(now() >= assets_info->open_buytime && now() < assets_info->open_extracttime , "open_buytime or open_extracttime not yet arrived");

    //不允许项目方和开发方购买tge
    eosio_assert(from != assets_info->pro_account, "This account is pro_account");
    eosio_assert(from != assets_info->forward_account, "This account is forward_account");

    bancorsets bancorsets_table(_self, _self.value);
    auto bancorsets_info = bancorsets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(bancorsets_info != bancorsets_table.end(), "bancorsets_info is not define");
    eosio_assert(bancorsets_info->bancor_balance.symbol == quantity.symbol, "quantity.symbol is error");
    eosio_assert(bancorsets_info->token_contract == code, "quantity contract is error");
    
    uint64_t amount = quantity.amount / pow(10, 4);
    eosio_assert(amount >= assets_info->min_quantity, "quantity less than min_quantity");
    uint64_t Examount = (amount*1.0) * (assets_info->exchangerd *1.0 /1000.0) * pow(10, 4);
   
    virtualsets virtualsets_table(_self, _self.value);
    auto virtualsets_info = virtualsets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(virtualsets_info != virtualsets_table.end(), "virtualsets is not defined");
    asset Send_ExChange = asset(Examount,virtualsets_info->virtua_currency.symbol);
    eosio_assert(virtualsets_info->virtua_totbuy >= Send_ExChange,"not enough!");
    virtualsets_table.modify(virtualsets_info, _self, [&](auto& st){
        st.virtua_totbuy = st.virtua_totbuy - Send_ExChange;
    });

    auto icoaccount_d = quantity.amount * 1.0 / 10000; 
    uint64_t forward_reward = icoaccount_d * 0.05 * 10000;
    uint64_t bancor_reward = icoaccount_d * 0.15 * 10000;
    uint64_t pro_reward = icoaccount_d * 0.8 * 10000;

    string forward_JsonMemo = Json_Transformation("TgeMortageReward","",exchange_symbol);
    string bancor_JsonMemo = Json_Transformation("TgeRecharge","",exchange_symbol);
    string pro_JsonMemo = Json_Transformation("TgeMortageReward","",exchange_symbol);
    string user_JosonMemo = Json_Transformation("Transfer_mortage",shadow_recieve,exchange_symbol);

    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->forward_account, asset(forward_reward,quantity.symbol), forward_JsonMemo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->ban_account, asset(bancor_reward,quantity.symbol), bancor_JsonMemo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->pro_account, asset(pro_reward,quantity.symbol), pro_JsonMemo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {virtualsets_info->virtua_tokcontract, _self, from, Send_ExChange, user_JosonMemo});
}

void ExChange::Transfer_ProExtract(name from, asset quantity,string shadow_recieve,string exchange_symbol,string exchange_number,name code){
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity symbol name");
    eosio_assert(quantity.symbol.precision() == 4, "quantity symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0
    eosio_assert(exchange_number != "", "exchange_number is error"); //支付货币应该 大于0

    //总开关
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    assets assets_table(_self,_self.value);
    auto assets_info = assets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(assets_info != assets_table.end(), "This exchange_symbol is not define");
    eosio_assert(assets_info->extract_switch, "This token extract_switch is not open");
    eosio_assert(from == assets_info->pro_account, "This account is not pro_account");

    //只有在开放领取的时间段可以提取
    eosio_assert(now() >= assets_info->open_extracttime , "open_extracttime not yet arrived");

    bancorsets bancorsets_table(_self, _self.value);
    auto bancorsets_info = bancorsets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(bancorsets_info != bancorsets_table.end(), "bancorsets_info is not define");
    eosio_assert(bancorsets_info->bancor_balance.symbol == quantity.symbol, "quantity.symbol is error");
    eosio_assert(bancorsets_info->token_contract == code, "quantity contract is error");

    asset ExtractAsset = stringToasset(exchange_number,exchange_symbol);
    eosio_assert(ExtractAsset.is_valid(), "invalid buyasset");
    eosio_assert(ExtractAsset.symbol.is_valid(), "invalid ExtractAsset symbol name");  
    eosio_assert(ExtractAsset.amount > 0,"ExtractAsset is error");
    eosio_assert(ExtractAsset.symbol.precision() == 4, "ExtractAsset symbol precision mismatch");//判断代币必须为小数点４位

    auto Extract_amount = ExtractAsset.amount *1.0/pow(10, ExtractAsset.symbol.precision());   
    auto current_smart_supply = bancorsets_info->bancor_supply.amount * 1.0 /pow(10, bancorsets_info->bancor_supply.symbol.precision());
    auto current_balance = bancorsets_info->bancor_balance.amount * 1.0 / pow(10, bancorsets_info->bancor_balance.symbol.precision());
    auto ratio = bancorsets_info->bancor_radio * 1.0;
    double to_tokens = current_balance *(1-pow(1-Extract_amount/current_smart_supply,1/(ratio/1000.0)));
    double deposit = current_balance *(1-pow(1-Extract_amount/current_smart_supply,1/(ratio/1000.0))) * 0.05;

    uint64_t send_to_tokens = to_tokens * pow(10, quantity.symbol.precision());
    uint64_t send_deposit = deposit * pow(10, quantity.symbol.precision());

    //eosio_assert(0,("quantity Not equal to send_to_tokens " + std::to_string(asset(send_to_tokens + send_deposit,quantity.symbol).amount)).c_str());
    eosio_assert(quantity == asset(send_to_tokens + send_deposit,quantity.symbol),"quantity equal to send_to_tokens");

    tgepros tgepros_table(_self,_self.value);
    auto tgepros_info = tgepros_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(tgepros_info != tgepros_table.end(), "tgepros_info is not defined");
    eosio_assert(tgepros_info->pro_totbuy >= ExtractAsset, "pro_totbuy less than ExtractAsset");

    tgepros_table.modify(tgepros_info, eosio::same_payer, [&](auto& st) {
        st.pro_totbuy -= ExtractAsset;
    });

    eosio_assert(bancorsets_info->bancor_supply.amount >= ExtractAsset.amount, "bancor_supply less than ExtractAsset");
    eosio_assert(bancorsets_info->bancor_balance.amount >= quantity.amount, "bancor_balance less than quantity");
    bancorsets_table.modify(bancorsets_info, eosio::same_payer, [&](auto& st) {
        st.bancor_supply.amount -= ExtractAsset.amount;
        st.bancor_balance.amount -= quantity.amount;
    });

    string Json_Memo =  Json_Transformation("Transfer_ProExtract",shadow_recieve,tgepros_info->pro_totbuy.symbol.code().to_string());
    string bancor_JsonMemo = Json_Transformation("TgeRecharge","",exchange_symbol);
    string forward_JsonMemo = Json_Transformation("TgeMortageReward","",exchange_symbol);


    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {assets_info->token_contract, _self, from, ExtractAsset, Json_Memo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->forward_account, asset(send_deposit,quantity.symbol), forward_JsonMemo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->ban_account, asset(send_to_tokens,quantity.symbol), bancor_JsonMemo});
}

void ExChange::Transfer_ForwardExtract(name from, asset quantity,string shadow_recieve,string exchange_symbol,string exchange_number,name code){
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity symbol name");
    eosio_assert(quantity.symbol.precision() == 4, "quantity symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0
    eosio_assert(exchange_number != "", "exchange_number is error"); //支付货币应该 大于0

    //总开关
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    assets assets_table(_self,_self.value);
    auto assets_info = assets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(assets_info != assets_table.end(), "This exchange_symbol is not define");
    eosio_assert(assets_info->extract_switch, "This token extract_switch is not open");
    eosio_assert(from == assets_info->forward_account, "This account is not forward_account"); 

    //只有在开放领取的时间段可以提取
    eosio_assert(now() >= assets_info->open_extracttime , "open_extracttime not yet arrived");

    bancorsets bancorsets_table(_self, _self.value);
    auto bancorsets_info = bancorsets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(bancorsets_info != bancorsets_table.end(), "bancorsets_info is not define");
    eosio_assert(bancorsets_info->bancor_balance.symbol == quantity.symbol, "quantity.symbol is error");
    eosio_assert(bancorsets_info->token_contract == code, "quantity contract is error");

    asset ExtractAsset = stringToasset(exchange_number,exchange_symbol);
    eosio_assert(ExtractAsset.is_valid(), "invalid buyasset");
    eosio_assert(ExtractAsset.symbol.is_valid(), "invalid ExtractAsset symbol name");    
    eosio_assert(ExtractAsset.amount > 0,"ExtractAsset is error");
    eosio_assert(ExtractAsset.symbol.precision() == 4, "ExtractAsset symbol precision mismatch");//判断代币必须为小数点４位

    auto Extract_amount = ExtractAsset.amount *1.0/pow(10, ExtractAsset.symbol.precision());   
    auto current_smart_supply = bancorsets_info->bancor_supply.amount * 1.0 /pow(10, bancorsets_info->bancor_supply.symbol.precision());
    auto current_balance = bancorsets_info->bancor_balance.amount * 1.0 / pow(10, bancorsets_info->bancor_balance.symbol.precision());
    auto ratio = bancorsets_info->bancor_radio * 1.0;
    double to_tokens = current_balance *(1-pow(1-Extract_amount/current_smart_supply,1/(ratio/1000.0)));
    double deposit = current_balance *(1-pow(1-Extract_amount/current_smart_supply,1/(ratio/1000.0))) * 0.05;

    uint64_t send_to_tokens = to_tokens * pow(10, quantity.symbol.precision());
    uint64_t send_deposit = deposit * pow(10, quantity.symbol.precision());

    //eosio_assert(0,("quantity Not equal to send_to_tokens " + std::to_string(asset(send_to_tokens + send_deposit,quantity.symbol).amount)).c_str());

    eosio_assert(quantity == asset(send_to_tokens + send_deposit,quantity.symbol),"quantity Not equal to send_to_tokens");

    tgeforwards tgeforwards_table(_self,_self.value);
    auto tgeforwards_info = tgeforwards_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(tgeforwards_info != tgeforwards_table.end(), "tgepros_info is not defined");
    eosio_assert(tgeforwards_info->forward_totbuy >= ExtractAsset, "forward_totbuy less than ExtractAsset");

    tgeforwards_table.modify(tgeforwards_info, eosio::same_payer, [&](auto& st) {
        st.forward_totbuy -= ExtractAsset;
    });

    eosio_assert(bancorsets_info->bancor_supply.amount >= ExtractAsset.amount, "bancor_supply less than ExtractAsset");
    eosio_assert(bancorsets_info->bancor_balance.amount >= quantity.amount, "bancor_balance less than quantity");
    bancorsets_table.modify(bancorsets_info, eosio::same_payer, [&](auto& st) {
        st.bancor_supply.amount -= ExtractAsset.amount;
        st.bancor_balance.amount -= quantity.amount;
    });

    string Json_Memo =  Json_Transformation("Transfer_ForwardExtract",shadow_recieve,tgeforwards_info->forward_totbuy.symbol.code().to_string());
    string bancor_JsonMemo = Json_Transformation("TgeRecharge","",exchange_symbol);
    string forward_JsonMemo = Json_Transformation("TgeMortageReward","",exchange_symbol);

    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {assets_info->token_contract, _self, from, ExtractAsset, Json_Memo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->forward_account, asset(send_deposit,quantity.symbol), forward_JsonMemo});
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {name(Basics_Symbol_Contract), _self, assets_info->ban_account, asset(send_to_tokens,quantity.symbol), bancor_JsonMemo});        
}

void ExChange::Transfer_Extract(name from, asset quantity,string shadow_recieve,string exchange_symbol,name code){
    eosio_assert(quantity.is_valid(), "invalid quantity");      //判断支付货币是否可用
    eosio_assert(quantity.symbol.is_valid(), "invalid quantity symbol name");
    eosio_assert(quantity.symbol.precision() == 4, "quantity symbol precision mismatch");//判断代币必须为小数点４位
    eosio_assert(quantity.amount > 0, "zero quantity is disallowed"); //支付货币应该 大于0

    //总开关
    switchs switch_table(_self, _self.value);
    bool switch_exists = switch_table.exists();
    eosio_assert(switch_exists, "switch is not define");
    auto switchs = switch_table.get();
    eosio_assert(switchs.switch_lock, "switch is not open");

    assets assets_table(_self,_self.value);
    auto assets_info = assets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(assets_info != assets_table.end(), "This exchange_symbol is not define");
    eosio_assert(assets_info->extract_switch, "This token extract_switch is not open");

    //不允许项目方和开发方购买提取
    eosio_assert(from != assets_info->pro_account, "This account is pro_account");
    eosio_assert(from != assets_info->forward_account, "This account is forward_account");

    //只有在开放领取的时间段可以提取
    eosio_assert(now() >= assets_info->open_extracttime , "open_extracttime not yet arrived");
    virtualsets virtualsets_table(_self, _self.value);
    auto virtualsets_info = virtualsets_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(virtualsets_info != virtualsets_table.end(), "virtualsets is not defined");
    eosio_assert(virtualsets_info->virtua_totbuy.symbol == quantity.symbol, "This quantity is not defined");
    eosio_assert(virtualsets_info->virtua_tokcontract == code, "This contract is not error");

    tgeusers tgeusers_table(_self,_self.value);
    auto tgeusers_info = tgeusers_table.find(symbol_code(exchange_symbol).raw());
    eosio_assert(tgeusers_info != tgeusers_table.end(), "tgeusers_info is not defined");
    eosio_assert(tgeusers_info->user_totbuy.amount >= quantity.amount, "user_totbuy less than quantity");

    tgeusers_table.modify(tgeusers_info, eosio::same_payer, [&](auto& st) {
        st.user_totbuy.amount -= quantity.amount;
    });

    asset Send_ExChange = asset(quantity.amount,tgeusers_info->user_totbuy.symbol);
    string Json_Memo =  Json_Transformation("Transfer_extract",shadow_recieve,tgeusers_info->user_totbuy.symbol.code().to_string());
    SEND_INLINE_ACTION(*this, inlinetransf, {_self,"active"_n}, {assets_info->token_contract, _self, from, Send_ExChange, Json_Memo});
}

//重置某代币开始购买和兑换的时间
ACTION ExChange::resettime(string symbol,uint64_t open_buytime,uint64_t open_extracttime,name executor){
    Identification_status(executor);
    eosio_assert(open_extracttime - open_buytime >= 86400, "closetime is error");

    assets assets_table(_self,_self.value);
    auto assets_info = assets_table.find(symbol_code(symbol).raw());
    eosio_assert(assets_info != assets_table.end(), "This quantity is not define");
    
    assets_table.modify(assets_info, eosio::same_payer, [&](auto& st) {
        st.open_buytime = open_buytime;
        st.open_extracttime = open_extracttime;
    });
}

//重置某代币买和提取的开关
ACTION ExChange::rstblswitch(string symbol,bool buy_switch,bool extract_switch,name executor){
    Identification_status(executor);
    assets assets_table(_self,_self.value);
    auto assets_info = assets_table.find(symbol_code(symbol).raw());
    eosio_assert(assets_info != assets_table.end(), "This quantity is not define");

    assets_table.modify(assets_info, eosio::same_payer, [&](auto& st) {
        st.buy_switch = buy_switch;
        st.extract_switch = extract_switch;
    });
}

//添加可执行权限
ACTION ExChange::addwhitelist(name whitename){
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


string ExChange::Json_Transformation(string type,string shadow_recieve,string symbol){
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

ACTION ExChange::inlinetransf(name contract, name from, name to, asset quantity, string memo){
    require_auth(_self);

    //EMIT_CONVERSION_EVENT("action:", contract.to_string(),"-----------------------");

    action(
        permission_level{ _self, name("active") },
        contract, name("transfer"),
        std::make_tuple(from, to, quantity, memo)
    ).send();
}


asset ExChange::get_balance(name contract, name owner, symbol_code sym) {
    accounts accountstable(contract, owner.value);
    const auto& ac = accountstable.get(sym.raw());
    return ac.balance;
}

// returns a token supply
asset ExChange::get_supply(name contract, symbol_code sym) {
    stats statstable(contract, sym.raw());
    const auto& st = statstable.get(sym.raw());
    return st.supply;
}

void ExChange::transfer(name from, name to, asset quantity, string memo){
    if (from == _self) {
        // TODO: prevent withdrawal of funds
        return;
    }

    if (to != _self) 
        return;

    //新添加Json解析字符串****************************************************
    cJSON *json=0,*json_type=0,*json_symbol=0,*json_shadow_rec=0,*json_number=0;
	json = cJSON_Parse(memo.c_str());
    json_type = cJSON_GetObjectItem(json, "type");
    json_symbol = cJSON_GetObjectItem(json, "symbol");
    json_number = cJSON_GetObjectItem(json, "number");
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
    string number = "";
    if(json_shadow_rec){
        if(json_shadow_rec->type != cJSON_String)return;
        shadow_recieve = json_shadow_rec->valuestring;
    }

    if(json_number){
        if(json_number->type != cJSON_String)return;
        number = json_number->valuestring;
    }

    cJSON_Delete(json);	

    if(json_type_str == "mortage"){
        Transfer_Mortage(from,quantity,shadow_recieve,json_symbol_str,_code);
    }
    else if(json_type_str == "extract"){
        Transfer_Extract(from,quantity,shadow_recieve,json_symbol_str,_code);
    }
    else if(json_type_str == "pro_extract"){
        Transfer_ProExtract(from,quantity,shadow_recieve,json_symbol_str,number,_code);
    }
    else if(json_type_str == "forward_extract"){
        Transfer_ForwardExtract(from,quantity,shadow_recieve,json_symbol_str,number,_code);
    }
    //********************************************************************** 
}

extern "C" {
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (action == "transfer"_n.value && code != receiver) {
            eosio::execute_action(eosio::name(receiver), eosio::name(code), &ExChange::transfer);
        }
        if (code == receiver) {
            switch (action) { 
                EOSIO_DISPATCH_HELPER(ExChange, (addassets)(addbancor)(addvirtual)(resettime)(rstblswitch)(initswitch)(resetswitch)
                (addwhitelist)(inlinetransf)) 
            }    
        }
        eosio_exit(0);
    }
}