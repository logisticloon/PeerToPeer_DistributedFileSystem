#!/bin/sh

#kill processes which are bound to port

p1 = $(lsof -t -i:4001)
p2 = $(lsof -t -i:5001)
p3 = $(lsof -t -i:4002)
p4 = $(lsof -t -i:5002)

if [-z $p1]
then
    /bin/kill p1
fi

if [-z $p2]
then
    /bin/kill p2
fi

if [-z $p3]
then
    /bin/kill p3
fi

if [-z $p4]
then
    /bin/kill p4
fi


 

