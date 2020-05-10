from dataclasses import dataclass
import matplotlib.pyplot as plt
import numpy as np

emitterCount = 2
frameCount = 3
multipleQueues = True

def visualizeAverageUpdateTimes(times, endParticleCount):
    return

def visualizeTotalExecutionTimes(times, endParticleCount):
    return

# Each emitter has frameCount * 2 timestamps

# def getMinMaxTimestamps(emitterValues):
#     minTimestamp = np.iinfo(np.uint64).max
#     maxTimestamp = 0
#     maxDelta = 0

#     for emitterIdx in range(len(emitterValues)):
#         for i in range(emitterCount):
#             delta = emitterValues[emitterIdx][frameCount - 1] - emitterValues[i][0]
#             if delta > maxDelta:
#                 maxDelta = delta
#                 minTimestamp = emitterValues[i][0]
#                 maxTimestamp = emitterValues[emitterIdx][frameCount - 1]

#     return (minTimestamp, maxTimestamp)



def fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli):
    for emitterIdx in range(len(emitterValues)):
        minTimestamp = emitterValues[emitterIdx][0]
        for timestampIdx in range(0, len(emitterValues[emitterIdx]) - 1, 2):
            timestamp1 = emitterValues[emitterIdx][timestampIdx + 1]
            timestamp0 = emitterValues[emitterIdx][timestampIdx]

            if timestamp1 < timestamp0:
                print("1st larger than 2nd: {} vs {}".format(timestamp0, timestamp1))

            duration    = timestamp1 - timestamp0
            duration    *= timestampToMilli
            startTime   = timestamp0 - minTimestamp
            startTime   *= timestampToMilli
            emitterTimes[emitterIdx].append((startTime, duration))

def readResultsFile():
    emitterTimes = []
    for _ in range(emitterCount):
        emitterTimes.append([])
    emitterValues   = [[]] * emitterCount

    # Read values
    file = open("results.txt", "rb")
    timestampToMilli = np.frombuffer(file.read(8), dtype=np.double)[0]
    totalTime = np.frombuffer(file.read(8), dtype=np.double)[0]
    for emitterIdx in range(emitterCount):
        emitterValues[emitterIdx] = np.frombuffer(file.read(8 * 2 * frameCount), dtype=np.uint64)

    # minMaxGPU = getMinMaxTimestamps(emitterValues)
    # totalTime = (minMaxGPU[1] - minMaxGPU[0]) * timestampToMilli
    # totalTime = (emitterValues[0][emitterCount - 1] - emitterValues[0][0]) * timestampToMilli

    fillEmitterTimes(emitterValues, emitterTimes, timestampToMilli)

    results = {
        "TotalTime": totalTime,
        "EmitterTimes": emitterTimes
    }

    return results

def calculateTotalExecutionTime(emitterTimes):
    return 0

def calculateAverageUpdateTime(emitterTimes):
    return 0

def startApplication(particleCount):
    return

def main():
    endParticleCount        = 5000000
    particleCountIncrement  = 100000

    testCount = endParticleCount / particleCountIncrement
    averageUpdateTimes  = [] * testCount
    totalExecutionTimes = [] * testCount

    particleCount = particleCountIncrement

    for testNr in range(testCount):
        startApplication(particleCount)
        results                = readResultsFile()
        emitterTimes = results["EmitterTimes"]
        averageUpdateTimes[testNr]  = calculateAverageUpdateTime(emitterTimes)
        totalExecutionTimes[testNr] = calculateTotalExecutionTime(emitterTimes)

        particleCount   += particleCountIncrement

    visualizeAverageUpdateTimes(averageUpdateTimes, endParticleCount)
    visualizeTotalExecutionTimes(totalExecutionTimes, endParticleCount)


if __name__ == "__main__":
    main()
