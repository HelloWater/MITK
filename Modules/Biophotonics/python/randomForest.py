# -*- coding: utf-8 -*-
"""
Created on Fri Feb  6 10:49:45 2015

@author: wirkert
"""


import numpy as np
import monteCarloHelper as mch
import matplotlib.pyplot as plt
from sklearn import tree
from sklearn.ensemble import RandomForestRegressor


# todo we:
# 2. optimization

# what to find out in this study:
# 1. band selection results
# 2. effect of normalizations
# 3. parameter study
# 4. optimal image quotient

#%% initialize


# the folder with the reflectance spectra
dataFolder = 'data/output/'

# load data
trainingParameters   = np.load(dataFolder + "2015February0511:02PMparamters2D.npy")
trainingReflectances = np.load(dataFolder + "2015February0511:02PMreflectances2D.npy")

testParameters   = np.load(dataFolder + "2015February1107:43PMparamtersRandom2D.npy")
testReflectances = np.load(dataFolder + "2015February1107:43PMreflectancesRandom2D.npy")



# normalize data
#trainingReflectances = trainingReflectances[:,[0, 1, 4, 9, 11, 13, 18, 22]]
#trainingReflectances = trainingReflectances[:,[  0,   3,   6,   9,  12,  15,  18,  22]]
#trainingReflectances = trainingReflectances[:,[  0,   1,   2,   3,  4,  5,  6,  7]]
trainingReflectances = mch.normalizeImageQuotient(trainingReflectances, iqBand=4)

#%% train forest

rf = RandomForestRegressor(50, max_depth=8, max_features="log2", n_jobs=5)
rf.fit(trainingReflectances, trainingParameters)

#with open("iris.dot", 'w') as f:
#    f = tree.export_graphviz(rf, out_file=f)

#%% test


#testReflectances = testReflectances[:,[0, 1, 4, 9, 11, 13, 18, 22]]
#testReflectances = testReflectances[:,[  0,   3,   6,   9,  12,  15,  18,  22]]
#testReflectances = testReflectances[:,[0, 1, 2, 3, 4, 5, 6, 7]]
testReflectances = mch.normalizeImageQuotient(testReflectances, iqBand=4)

# predict test reflectances and get absolute errors.
absErrors = np.abs(rf.predict(testReflectances) - testParameters)

print("error distribution BVF, Volume fraction")
print("median: " + str(np.median(absErrors, axis=0)))
print("lower quartile: " + str(np.percentile(absErrors, 25, axis=0)))
print("higher quartile: " + str(np.percentile(absErrors, 75, axis=0)))
