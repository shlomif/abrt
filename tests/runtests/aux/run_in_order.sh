#!/bin/bash

testlist=$(cat $TEST_LIST | grep '^[^#]\+$')
crit_test_fail=0

for test_dir in $testlist; do
    test="$test_dir/runtest.sh"
    testname="$(grep 'TEST=\".*\"' $test | awk -F '=' '{ print $2 }' | sed 's/"//g')"
    short_testname=${testname:0:55}

    syslog $short_testname
    mkdir -p "$OUTPUT_ROOT/test/$testname"
    logfile="$OUTPUT_ROOT/test/$testname/full.log"
    TESTNAME=$testname LOGFILE=$logfile $RUNNER_SCRIPT $test > $logfile

    # check test result
    test_result="FAIL"
    if [ -e $logfile ]; then
        if ! grep -q FAIL $logfile; then
            test_result="PASS"
        fi
    fi

    # console reporting
    if [ "$test_result" == "FAIL" ]; then
        touch "$OUTPUT_ROOT/test/$testname/failed"
        echo_failure
    else
        echo_success
    fi
    echo " | $short_testname"

    if [ "$test_result" == "FAIL" ]; then
        for ctest in $TEST_CRITICAL; do
            if [ "$test" == "$ctest/runtest.sh" ]; then
                echo_failure
                echo -n " | Critical test failed"
                if [ "$TEST_CONTINUE" = "0" ]; then
                    echo ", stopping further testing"
                    crit_test_fail=1
                    break
                else
                    echo ", TEST_CONTINUE in effect, resuming testing"
                fi
            fi
        done
        if [ $crit_test_fail -eq 1 ]; then
            break
        fi
    fi

done

if grep -q FAIL $OUTPUT_ROOT/results; then
    RESULT="FAIL"
else
    RESULT="PASS"
fi

if [ $crit_test_fail -eq 1 ]; then
    RESULT="FAIL"
fi

export RESULT
