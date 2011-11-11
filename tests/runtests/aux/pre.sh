#!/bin/bash

export > $OUTPUT_ROOT/pre/envs.log
cp /var/log/messages $OUTPUT_ROOT/pre/messages

yum install -y beakerlib dejagnu btparser-devel time createrepo mock
