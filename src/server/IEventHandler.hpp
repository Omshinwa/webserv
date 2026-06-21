#ifndef IEVENTHANDLER_H
#define IEVENTHANDLER_H

class IEventHandler {
    public:
        virtual ~IEventHandler() {}
        virtual void on_readable(int fd) = 0;
        virtual void on_writable(int fd) = 0;
        virtual void on_hangup(int fd) = 0;
};

#endif
