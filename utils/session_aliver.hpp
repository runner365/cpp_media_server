#ifndef SESSION_ALIVER_HPP
#define SESSION_ALIVER_HPP
#include <stdint.h>
#include <stddef.h>

class session_aliver
{
public:
    session_aliver(int try_max = 4):try_max_(try_max) {
    }
    ~session_aliver() {

    }

public:
    void keep_alive() {
        not_alive_cnt_ = 0;
    }

    bool is_alive() {
        if (not_alive_cnt_++ > try_max_) {
            return false;
        }
        return true;
    }

private:
    int not_alive_cnt_ = 0;
    int try_max_ = 0;
};

#endif