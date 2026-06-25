/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** CircularBuffer
*/


#ifndef INCLUDED_CIRCULARBUFFER_HPP
    #define INCLUDED_CIRCULARBUFFER_HPP

#include <cstddef>
#include <string>

class CircularBuffer {
    public:
        static constexpr size_t CAPACITY = 8192;

        CircularBuffer() : _head(0), _tail(0), _size(0) {}

        //return false if data would overflow (data is dropped on overflow)
        bool write(const char *data, size_t n);
        //fills out with the next line (strips \r\n)
        //returns false if no complete line is available yet
        bool read_line(std::string &out);

        size_t free_space() const { return CAPACITY - _size; }

    private:
        char _buf[CAPACITY];
        size_t _head;
        size_t _tail;
        size_t _size;
};

#endif
