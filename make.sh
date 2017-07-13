#!/bin/bash

make 2> compile_error
less compile_error
