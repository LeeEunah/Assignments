from math import log


def readData(fileName):
    datas = []
    inputfile = open("./"+fileName, 'r')
    firstLine = inputfile.readline()
    attributes = firstLine[:-1].split('\t')
    
    lines = inputfile.readlines()
    for line in lines:
        divide = line[:-1].split('\t')
        datas.append(divide)

    inputfile.close()
    return datas, attributes


def entropy(datas):
    dataSize = len(datas)
    classLabel = {}
    entropy = 0.0

    for data in datas:
        currentLabel = data[-1]
        if currentLabel not in classLabel.keys():
            classLabel[currentLabel] = 0
        classLabel[currentLabel] += 1

    for label in classLabel:
        probability = float(classLabel[label]/dataSize)
        entropy -= probability * log(probability, 2)
    return entropy


def pickAttribute(datas, index, attribute):
#    print(attribute)
    pickData = []
    for data in datas:
        if data[index] == attribute:
            pickData.append(data[:])
#    print(len(pickData))
    return pickData            


def information_gain(datas, index):
    dataSize = len(datas)
    attributeCount = {}
    weightedEntropy = 0.0
    splitInfo = 0.0

    for data in datas: 
        currentAttribute = data[index]
        if currentAttribute not in attributeCount.keys():
            attributeCount[currentAttribute] = 0
        attributeCount[currentAttribute] += 1

    for attribute in attributeCount:
        print('attri: ', attribute)
        probability = float(attributeCount[attribute]/dataSize)
        partOfData = pickAttribute(datas,index, attribute)
##        if len(attributeCount) > 2:
##            weightedEntropy -= probability * log(probability, 2)
##            print('weight: ', weightedEntropy)
##
##        else:
        weightedEntropy += probability * entropy(partOfData)

        gain = entropy(datas) - weightedEntropy

        if len(attributeCount) > 1:
            splitInfo -= probability * log(probability, 2)

    gainRatio = gain / splitInfo
#    print('gainRatio: ', gainRatio)

    return gainRatio


def selectBestAttribute(bestGain, gainRatio, index, bestIndex):
    if bestGain < gainRatio:
        print('min')
        bestGain = gainRatio
        bestIndex = index
    
    return bestGain, bestIndex




# main
datas, attributes = readData("dt_train.txt")
#print(entropy(datas))
bestGain = 0.0
bestIndex = 0
for i in range (len(attributes) -1):
    gainRatio = information_gain(datas, i)
    bestGain, bestIndex = selectBestAttribute(bestGain, gainRatio, i, bestIndex)

# Decided root node by comparing the gainRatio so far


