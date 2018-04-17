import sys
from math import log

class Node:
    def __init__(self, index=-1, value=None, leaf=None, left=None, right=None):
        self.index = index 
        self.value = value
        self.leaf = leaf
        self.left = left
        self.right = right


def createTree(datas, attributes):
    weightedEntropy = 0.0
    rmAttribute = attributes[:-1]
    bestGain = 0.0
    
    for i in range (len(rmAttribute)):
        attributeCount = {}
        
        for data in datas:
            attributeCount[data[i]] = 1

        for value in attributeCount:
            left, right = pickAttribute(datas, i, value)

            probability = float(len(left)/len(datas))

            weightedEntropy = probability * entropy(left) + (1-probability) * entropy(right)
            gain = entropy(datas) - weightedEntropy

            
            if bestGain < gain:
                bestGain = gain
                bestIndex = i
                bestValue = value
                bestLeft = left
                bestRight = right

    if bestGain > 0:
        leftNode = createTree(bestLeft, attributes)
        rightNode = createTree(bestRight, attributes)
        return Node(bestIndex, bestValue, None, leftNode, rightNode)

    else:
        classLabel = {}
        for data in datas:
            currentLabel = data[-1]
            if currentLabel not in classLabel.keys():
                classLabel[currentLabel] = 0
            classLabel[currentLabel] += 1

        return Node(None, None, classLabel, None, None)
            


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


def pickAttribute(datas, index, value):
    pickData = []
    notPickData = []

    for data in datas:
        if data[index] == value:
            pickData.append(data[:])
        else:
            notPickData.append(data[:])
    return pickData, notPickData


def recursiveTest(data, tree):
    if tree.leaf == None:
        temp = data[tree.index]
        node = None
        if temp == tree.value:
            node = tree.left
        else:
            node = tree.right
        return recursiveTest(data, node)
    else:
        return tree.leaf


def testTree(testData, tree):
    classLabel = []
    for data in testData:
        tested = recursiveTest(data, tree)
        listTest = list(tested)
        classLabel.append(listTest[0])
    return classLabel


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


def writeData(fileName, attributes, testDatas, classLabel):
    word = ""
    file = open("./" + fileName, "w")
    for i in range (len(attributes)):
        word += attributes[i]
        word += '\t'
        if i == len(attributes) -1:
            word +='\n'

    for j in range (len(testDatas)):
        for i in range (len(testDatas[j])):
            word += testDatas[j][i]
            word += '\t'
            if i == len(testDatas[j]) -1:
                word += classLabel[j]
                word += '\n'

    file.write(word)
    
    file.close()






# main
if len(sys.argv) != 4:
    print('''Please fill in the command form.
Executable_file minimum_support inputfile outputfile''')
    exit()

trainFile = sys.argv[1]
testFile = sys.argv[2]
outputFile = sys.argv[3]

datas, attributes = readData(trainFile)
tree = createTree(datas, attributes)
testDatas, testAttributes = readData(testFile)
classLabel = testTree(testDatas, tree)
writeData(outputFile, attributes, testDatas, classLabel)
