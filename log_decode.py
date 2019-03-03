#!/usr/bin/python3
'''
Pass log file as arg.
'''
import sys
import re
from matplotlib import pyplot

fname = sys.argv[1]

def get_val(line):
    p = re.compile('<([a-f 0-9]+)>')
    m = p.search(line)
    if not m:
        print("Failed to find a match")
        return ''

    return str(m.group(1))


def calc_rolling_power(powers, seconds, window):
    '''
    '''
    chunks = []
    last_second = 0
    for (second, power) in zip(seconds, powers):
        # Make a list of lists. Each nested list is a bucket for samples from a second.
        if second > last_second:
            # New bucket
            chunks.append([])
        chunks[-1].append(power)

    averages = []
    samples = []
    # Now use the time buckets to calculate rolling average
    for chunk in chunks:
        # If we have less than the window, average all
        # If more than the window, throw out oldest and average
        samples.append(sum(chunk) / len(chunk))
        if len(samples) > window:
            samples = samples[window-1:]
        # Length of samples should be the window size (or less)
        averages.append(sum(samples) / len(samples))

    return averages

'''
Here's what the stuff looks like at the moment:

      blePublishLog("%.1f %.1f|%d=%d", avgForce, mps, cadence, power);

      blePublishLog("%d: %d polls", millis() / 1000, numPolls);
'''
if __name__ == '__main__':
    seconds = []
    forces = []
    cadence = []
    powers = []

    with open(fname, 'r') as f:
        force_line = re.compile('([0-9]+\.[0-9])\s[0-9]+\.[0-9]\|([0-9]+)=(-?[0-9]+)')
        time_line = re.compile('([0-9]+):\s([0-9]+)')
        poll_line = re.compile('^F')
        for line in f.readlines():
            if 'notified' in line:
                # Then we know this is an update, but for what characteristic.
                if '(1234)' in line:
                    # BLE logger
                    msg = format(bytearray.fromhex(get_val(line)).decode())
                    force_match = force_line.search(msg)
                    time_match = time_line.search(msg)
                    poll_match = poll_line.search(msg)
                    if force_match:
                        forces.append(float(force_match.group(1)))
                        cadence.append(int(force_match.group(2)))
                        powers.append(int(force_match.group(3)))
                    elif time_match:
                        seconds.append(int(time_match.group(1)))
                    elif poll_match:
                        pass
                    else:
                        print('WARN: No match for data, "{}"'.format(msg))
                '''
                # These are a little harder because they're packed BLE characteristics.
                # Definitely worth pulling out but little more tedious.
                elif '(2A63)' in line:
                    # Power
                    pass
                elif '(2A5B)' in line:
                    # Cadence
                    pass
                '''

    # They different groups have to be same size, truncate.
    new_length = min(len(seconds), len(powers))

    three_sec_powers = calc_rolling_power(powers[:new_length], seconds[:new_length], 3)
    ten_sec_powers = calc_rolling_power(powers[:new_length], seconds[:new_length], 10)
    min_powers = calc_rolling_power(powers[:new_length], seconds[:new_length], 60)

    #pyplot.plot(seconds[:new_length], powers[:new_length], label='Instantaneous Power')
    pyplot.plot(seconds[:new_length], three_sec_powers, label='3 Second Power')
    pyplot.plot(seconds[:new_length], ten_sec_powers, label='10 Second Power')
    pyplot.plot(seconds[:new_length], min_powers, label='60 Second Power')
    pyplot.plot(seconds[:new_length], cadence[:new_length], label='Instant. Cadence')
    pyplot.plot(seconds[:new_length], forces[:new_length], label='Instant. Force')
    pyplot.legend()
    pyplot.show()
