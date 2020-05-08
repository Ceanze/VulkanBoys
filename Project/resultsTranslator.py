from dataclasses import dataclass
import matplotlib.pyplot as plt
import numpy as np

emitterCount = 10
frameCount = 100
multipleQueues = True

def visualizeTimeline(emitterTimes):
    yTickLabels = [""] * len(emitterTimes)

    barHeight = 2

    # Vertical space between each emitter's bar
    verticalSpacing = 1
    _, ax = plt.subplots()

    for emitterIdx in range(len(emitterTimes)):
        yTickLabels[emitterIdx] = "Emitter #{}".format(emitterIdx)

        barStartY = verticalSpacing * (emitterIdx + 1) + emitterIdx * barHeight

        ax.broken_barh(emitterTimes[emitterIdx], (barStartY, barHeight))

    #ax.set_ylim(5, 35)
    #ax.set_xlim(0, 200)
    ax.set_xlabel('Execution Time (ms)')
    #ax.set_yticks([15, 25])

    ax.set_yticklabels(yTickLabels)
    ax.grid(True)

    plt.show()

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
            if emitterValues[emitterIdx][timestampIdx + 1] < emitterValues[emitterIdx][timestampIdx]:
                print("1st larger than 2nd: {} vs {}".format(emitterValues[emitterIdx][timestampIdx + 1], emitterValues[emitterIdx][timestampIdx]))
                #emitterTimes[emitterIdx].append(0)
            #else:
            duration = emitterValues[emitterIdx][timestampIdx + 1] - emitterValues[emitterIdx][timestampIdx]
            duration *= timestampToMilli
            startTime = emitterValues[emitterIdx][timestampIdx] - minTimestamp
            startTime *= timestampToMilli
            emitterTimes[emitterIdx].append((startTime, duration))

def main():
    emitterValues   = [[]] * emitterCount
    emitterTimes    = [[]] * emitterCount

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
    visualizeTimeline(emitterTimes)


if __name__ == "__main__":
    main()
