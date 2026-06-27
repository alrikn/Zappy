/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** CircularBuffer
*/

#include "CircularBuffer.hpp"

bool CircularBuffer::write(const char *data, size_t n)
{
    if (n > free_space())
        return false; //check if we not overwriting line we haven't read yet
    //tail is next free space
    for (size_t i = 0; i < n; i++) {
        _buf[_tail] = data[i];
        _tail = (_tail + 1) % CAPACITY;
    }
    _size += n;
    return true;
}

bool CircularBuffer::read_line(std::string &out)
{
    for (size_t i = 0; i < _size; i++) {
        //head is next char to read
        if (_buf[(_head + i) % CAPACITY] == '\n') {
            out.clear();
            out.reserve(i); //reserve space for the line (excluding \n)
            for (size_t j = 0; j < i; j++)
                out += _buf[(_head + j) % CAPACITY];
            if (!out.empty() && out.back() == '\r')
                out.pop_back();
            // + 1 to skip \n
            _head = (_head + i + 1) % CAPACITY;
            _size -= i + 1;
            return true;
        }
    }
    return false;
}
