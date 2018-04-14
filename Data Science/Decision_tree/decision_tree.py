from math import log


def tree(datas, attributes):
    bestGain = 0.0
    bestIndex = 0

    print('attri: ', attributes)
    print('datas: ', datas)
# check best gain ratio among the attributes
#    for i in range (len(attributes)-1):
#        gainRatio = information_gain(datas, i)
#        bestGain, bestIndex = selectBestAttribute(bestGain, gainRatio, i, bestIndex)

    # Stop tree
    # when all of the values of label are equal
    if 
        
    bestGain, bestIndex = selectBestAttribute(datas, attributes)
#    print('bestIndex: ', bestIndex)
    
    bestAttribute = attributes[bestIndex]
    print('bestAttribute: ', bestAttribute)
    Tree = {bestAttribute : {}}
# gathering the label of attribute without duplicating

    kindOfValue = set([label[bestIndex] for label in datas])
    print('kind :', kindOfValue)
    
    del (attributes[bestIndex])
    for data in datas:
        del data[bestIndex]

#    print('subAttribute: ', attributes)
#    print('subData: ', datas)


# recursive
    for value in kindOfValue:
        subAttribute = attributes[:]
        subData = datas[:]
        Tree[bestAttribute][value] = tree(subData, subAttribute)
        print('Tree: ', Tree)

        

    


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

# the number of label in data
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
        #print('data:' ,data)
        if currentAttribute not in attributeCount.keys():
            attributeCount[currentAttribute] = 0
        attributeCount[currentAttribute] += 1

    for attribute in attributeCount:
#        print('attri: ', attribute)
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


def selectBestAttribute(datas, attributes):
    bestGain = 0.0
    for i in range (len(attributes)-1):
#        print('i: ', i)
        gainRatio = information_gain(datas, i)

        if bestGain < gainRatio:
            bestGain = gainRatio
            bestIndex = i

    
    return bestGain, bestIndex




# main
datas, attributes = readData("dt_train.txt")
#print(entropy(datas))




#print('data: ', datas)
#print('attributes: ', attributes)
# print('bestGain: ', bestGain)
#print('bestIndex: ', bestIndex)

#rmLastAttributes = attributes[:-1]
#rmLastDatas = [data[:-1] for data in datas]
tree(datas, attributes)
# Decided root node by comparing the gainRatio so far


