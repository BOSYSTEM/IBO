cmake_minimum_required(VERSION 3.5)
project(ExChange VERSION 1.1.0)

find_package(eosio.cdt)

### Generate the wasm and abi
add_contract( ExChange ExChange ExChange.cpp )

### add the path to where the ricardian contracts/clauses are found
target_compile_options( ExChange.wasm PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR} )