#include "MistwatchBankRocksPreview.h"
#include "MistwatchForestPlayable.h"
#include <iostream>
#include <stdexcept>

using Mistwatch::ForestPlayable::Argument;
using Mistwatch::ForestPlayable::SelectRiverbankArgument;
static_assert(SelectRiverbankArgument(L"",false,false)==Argument);
static_assert(SelectRiverbankArgument(L" \t ",false,false)==Argument);
static_assert(SelectRiverbankArgument(L"",true,false).empty());
static_assert(SelectRiverbankArgument(L"",false,true).empty());
static_assert(SelectRiverbankArgument(L"",true,true).empty());
static_assert(SelectRiverbankArgument(L" --prototype ",false,false).empty());
static_assert(SelectRiverbankArgument(Argument,false,false)==Argument);
static_assert(SelectRiverbankArgument(Mistwatch::BankRocksPreview::Argument,false,false)==Mistwatch::BankRocksPreview::Argument);
static_assert(SelectRiverbankArgument(L" --unknown ",false,false)==L"--unknown");

int main(){try{
    using Mistwatch::BankRocksPreview::Resolve;
    const auto need=[](bool b){if(!b)throw std::runtime_error("BANK_PREVIEW_MODE_CONTRACT");};
    need(!Resolve(false,false,false,L""));need(!Resolve(false,true,true,L""));
    need(Resolve(true,false,true,L""));need(Resolve(false,true,true,L"1"));unsigned rejected{};
    const auto bad=[&](bool manual,bool replay,bool forest,std::wstring_view value){bool failed=false;try{static_cast<void>(Resolve(manual,replay,forest,value));}catch(const std::invalid_argument&){failed=true;}need(failed);++rejected;};
    bad(true,true,true,L"");bad(true,false,true,L"1");bad(false,false,true,L"1");
    bad(false,true,false,L"1");bad(true,false,false,L"");bad(false,true,true,L"0");
    bad(false,true,true,L"11");bad(false,true,true,L"unknown");
    std::cout<<"BANK_ROCK_PREVIEW_MODE_PASS default_off=1 explicit_manual=1 explicit_validated_replay=1 negative_cases="<<rejected<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
