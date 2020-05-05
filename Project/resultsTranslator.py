import struct
import numpy as np

emitterCount = 10
frameCount = 100
multipleQueues = True

# Each emitter has frameCount * 2 timestamps

def getTotalTimeGPU(emitterValues):
    minTimestamp = np.iinfo(np.uint64).max
    maxTimestamp = 0

    for emitterIdx in range(len(emitterValues)):
        if emitterValues[emitterIdx][0] < minTimestamp:
            minTimestamp = emitterValues[emitterIdx][0]
        elif emitterValues[emitterIdx][frameCount - 1] > maxTimestamp:
            maxTimestamp = emitterValues[emitterIdx][frameCount - 1]

    return maxTimestamp - minTimestamp


def main():
    emitterValues = [[]]*emitterCount

    # Read values
    file = open("results.txt", "rb")
    timestampToMilli = np.frombuffer(file.read(8), dtype=np.float)[0]
    for emitterIdx in range(emitterCount):
        emitterValues[emitterIdx] = np.frombuffer(file.read(8 * 2 * frameCount), dtype=np.uint64)

    totalTimeGPU = getTotalTimeGPU(emitterValues)
    totalTime = totalTimeGPU * timestampToMilli
    print("Total time: " + str(totalTime))


if __name__ == "__main__":
    main()
