#!/bin/bash

python3 -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. ./csci4220_hw4.proto
