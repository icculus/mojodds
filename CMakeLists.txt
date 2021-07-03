# YOU PROBABLY DON'T HAVE TO USE THIS CMAKE PROJECT, IT'S JUST A SINGLE
#  C FILE. COPY IT INTO YOUR PROJECT INSTEAD.

project(mojodds)
cmake_minimum_required(VERSION 3.0.0)
add_library(mojodds STATIC mojodds.c)
