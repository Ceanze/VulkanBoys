from dataclasses import dataclass
import numpy as np

emitterCount = 10
frameCount = 100
multipleQueues = True

# Each emitter has frameCount * 2 timestamps

def getMinMaxTimestamps(emitterValues):
    minTimestamp = np.iinfo(np.uint64).max
    maxTimestamp = 0

    for emitterIdx in range(len(emitterValues)):
        if emitterValues[emitterIdx][0] < minTimestamp:
            minTimestamp = emitterValues[emitterIdx][0]
        elif emitterValues[emitterIdx][frameCount - 1] > maxTimestamp:
            maxTimestamp = emitterValues[emitterIdx][frameCount - 1]

    return (minTimestamp, maxTimestamp)

def fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli, minTimestamp):
    for emitterIdx in range(len(emitterValues)):
        for timestampIdx in range(0, len(emitterValues[emitterIdx]) - 1, 2):
            duration = emitterValues[emitterIdx][timestampIdx + 1] - emitterValues[emitterIdx][timestampIdx]
            duration *= timestampToMilli
            startTime = emitterValues[emitterIdx][timestampIdx] - minTimestamp
            startTime *= timestampToMilli
            emitterTimes[emitterIdx].append((startTime, duration))

def main():
    emitterValues = [[]]*emitterCount
    emitterTimes = [[]]*emitterCount

    # Read values
    file = open("results.txt", "rb")
    timestampToMilli = np.frombuffer(file.read(8), dtype=np.float)[0]
    for emitterIdx in range(emitterCount):
        emitterValues[emitterIdx] = np.frombuffer(file.read(8 * 2 * frameCount), dtype=np.uint64)

    minMaxGPU = getMinMaxTimestamps(emitterValues)
    totalTime = (minMaxGPU[1] - minMaxGPU[0]) * timestampToMilli

    fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli, minMaxGPU[0])

    print(emitterTimes[0])
    print("Total time: " + str(totalTime))


if __name__ == "__main__":
    main()
