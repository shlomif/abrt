#!/bin/bash

export > $OUTPUT_ROOT/pre/envs.log

yum install -y beakerlib dejagnu btparser-devel time createrepo mock
