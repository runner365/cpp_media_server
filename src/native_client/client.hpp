#ifndef NATIVE_CLIENT_HPP
#define NATIVE_CLIENT_HPP
#include <stdint.h>
#include <stddef.h>
#include <iostream>
#include <unistd.h>
#include <string>

class native_clientI
{
public:
    virtual int open_url(const std::string& url) = 0;
    virtual void run() = 0;
};

#endif