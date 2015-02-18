# -*- coding: utf-8 -*-
"""
Created on Fri Feb  6 16:14:57 2015

@author: wirkert
"""

import time
import datetime
import random

import numpy as np

print __name__

from helper import monteCarloHelper as mch
from setup import simulation
from setup import systemPaths


def noisyRandom(generatedFilename):


    infileString, outfolderMC, outfolderRS, gpumcmlDirectory, gpumcmlExecutable = systemPaths.initPaths()
    infile = open(infileString)

    BVFs, Vss, ds, SaO2s, rs, nrSamples, photons, wavelengths, FWHM, eHbO2, eHb, nrSimulations = simulation.noisy()


    reflectances  = np.zeros((nrSimulations, len(wavelengths)))
    parameters    = np.zeros((nrSimulations, 8))

    print('start simulations...')

    #%% start program logic
    start = time.time()



    for i in range(nrSimulations):

        print('starting simulation ' + str(i) + ' of ' + str(nrSimulations))

        BVF = random.uniform(min(BVFs), max(BVFs))
        Vs  = random.uniform(min(Vss), max(Vss))
        d   = random.uniform(min(ds), max(ds))
        r   = random.uniform(min(rs), max(rs))
        SaO2= random.uniform(min(SaO2s), max(SaO2s))

        sm_BVF = random.uniform(min(BVFs), max(BVFs))
        sm_Vs  = random.uniform(min(Vss), max(Vss))
        sm_SaO2= random.uniform(min(SaO2s), max(SaO2s))

        parameters[i,:] = np.array([BVF, Vs, d, r, SaO2, sm_BVF, sm_Vs, sm_SaO2])


        for j, wavelength in enumerate(wavelengths):

            reflectanceValue = mch.runOneSimulation(
                wavelength, eHbO2, eHb,
                infile, outfolderMC, gpumcmlDirectory, gpumcmlExecutable,
                BVF, Vs, d,
                r, SaO2,
                submucosa_BVF=sm_BVF, submucosa_Vs=sm_Vs, submucosa_SaO2=sm_SaO2,
                Fwhm = FWHM, nrPhotons=photons)


            print((BVF, Vs, d, SaO2, r, wavelength, sm_BVF, sm_Vs, sm_SaO2))

            # here, summarize result from wavelength in reflectance spectrum
            reflectances[i, j] = reflectanceValue




    infile.close()

    # save the reflectance results!
    now = datetime.datetime.now().strftime("%Y%B%d%I:%M%p")
    np.save(outfolderRS + now + generatedFilename + "reflectances" + str(photons) + "photons", reflectances)
    np.save(outfolderRS + now + generatedFilename  + str(nrSimulations) + "parameters", parameters)

    end = time.time()
    print "total time to generate noisy random data: " + str((end - start))