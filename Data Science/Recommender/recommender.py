from math import sqrt
import sys

def readTrainFile(fileName):
    Dtrain = {}
    trainFile = open("./"+fileName, 'r')
    lines = trainFile.readlines()
    for line in lines:
        (user, movie, rating, time) = line[:-1].split('\t')
        Dtrain.setdefault(user, {})
        Dtrain[user][movie] = float(rating)
    trainFile.close()
    return Dtrain
    
    

def readTestFile(fileName):
    datas = []
    testFile = open("./"+fileName, 'r')
    lines = testFile.readlines()
    for line in lines:
        data = line[:-1].split('\t')
        datas.append(data)

    testFile.close()
    return datas


# Calculate Pearson Correlation Coefficient(PCC) formula
# to find out similarity between neighbors and target user
def pearson(Dtrain, user1, user2):
    sumX = 0    # sum of user1's rating
    sumY = 0    # sum of user2's rating
    sumPowX = 0     # sum of square of user1's rating
    sumPowY = 0     # sum of square of user2's rating
    sumXY = 0       # sum of multiplying user1's rating and user2's rating
    cnt = 0     # the number of movie
    
    for movie in Dtrain[user1]:
        if movie in Dtrain[user2]:      # movies that user1 and user2 watched
            sumX += Dtrain[user1][movie]
            sumY += Dtrain[user2][movie]
            sumPowX += pow(Dtrain[user1][movie], 2)
            sumPowY += pow(Dtrain[user2][movie], 2)
            sumXY += Dtrain[user1][movie] * Dtrain[user2][movie]
            cnt += 1

    if cnt == 0:    # handle division by 0 error
        return 0

    square = sqrt((sumPowX - (pow(sumX, 2) / cnt)) * (sumPowY - (pow(sumY, 2) / cnt)))
    if square == 0:     # handle division by 0 error
        return 0
    
    pearson_formula = (sumXY - ((sumX * sumY) / cnt)) / square
    return pearson_formula


# Store similarity of neighbors to descending order
def neighborSimilarity(Dtrain, user1):
    sim_neighbor = []
    for user in Dtrain:
        if user == user1 : continue     # exclude myself
        sim_neighbor.append((pearson(Dtrain, user1, user), user))   # tuple type

    sim_neighbor.sort()
    sim_neighbor.reverse()  # descending order

#    print (sim_neighbor)
    return sim_neighbor
    

def predictRating(Dtrain, Dtest, target_user):
    score = 0
    sum_movie = {}
    sum_sim = {}
    target_rate = []
    sim_neighbor = neighborSimilarity(Dtrain, target_user)
    for (sim, neighbor) in sim_neighbor:
        if sim <= 0 : continue      
        for movie in Dtrain[neighbor]:  # if similarity is positive
            if movie not in Dtrain[target_user]:
                score += sim * Dtrain[neighbor][movie]
                sum_movie.setdefault(movie, 0)  # initialize
                sum_movie[movie] += score   # sum of movie rating of target user

                sum_sim.setdefault(movie, 0)
                sum_sim[movie] += sim
            score = 0

    for key_movie in sum_movie:
        # target rating = total rating / total similarity
        sum_movie[key_movie] = sum_movie[key_movie] / sum_sim[key_movie]
        target_rate.append((key_movie, sum_movie[key_movie]))

    target_rate.sort()
    return target_rate

    

        
def checkUser(Dtrain, Dtest):
    dup_check = set()
    result = []
    # the movie that user did not watch
    for (target_user, target_movie, target_rating, target_time) in Dtest:
        if target_user not in dup_check:        # check duplication of user
            target_row = predictRating(Dtrain, Dtest, target_user)
            
        dup_check.add(target_user)

        rate = [rating for (movie, rating) in target_row if movie == target_movie]
        if len(rate) == 0:
            rate = [2]
        result.append((target_user, target_movie, rate[0]))
    return result


def writeFile(result, title):
    word = ""
    for row in result:
        word += "%s\t%s\t%s\n" % (row[0], row[1], row[2])
    outputFile = open(title, 'w')
    outputFile.write(word)
    outputFile.close()
    


## main

# Check the command line arguments
if len(sys.argv) != 3:
    print('''Please fill in the command form.
Executable_file minimum_support inputfile outputfile''')
    exit()
input_base = sys.argv[1]
input_test = sys.argv[2]

Dtrain = readTrainFile(input_base)
Dtest = readTestFile(input_test)

result = checkUser(Dtrain, Dtest)
title = input_base + '_prediction.txt' 
writeFile(result, title)
