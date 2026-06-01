cmake_minimum_required(VERSION 3.10)
project(Financial-Data_Pipeline)
set(CMAKE_CXX_STANDARD 20)

#postgres related files
set(POSTGRES_SOURCES
    src/storage/postgres_client.cpp
)

#executable repos
add_executable(test_postgres
    tests/unit/test_quote_repo.cpp
    test/unit/test_tick_repo.cpp
)

target_link_libraries(test_postgres
    PRIVATE
    fincore_db
    gtest_main
    ${PostgreSQL_LIBRARIES} #libpq
    fmt::fmt
)




# Sources
#set(SOURCES
#        src/core/main.cpp
#        src/core/order_book.cpp
#        src/api/alpha_vantage_client.cpp
#        src/storage/redis_client.cpp
#)

# Create executable
#add_executable(market_pipeline ${SOURCES})
add_executable(test_utils
    tests/unit/test_validation.cpp
    tests/unit/test_time_utils.cpp
)

add_test(NAME PostgresUnit COMMAND test_postgres)


