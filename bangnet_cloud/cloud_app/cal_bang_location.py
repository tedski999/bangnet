import json
import numpy as np
from scipy.optimize import fsolve
from pyproj import Proj
import mysql.connector
from mysql.connector import errorcode

class TDoALocalization:
    def __init__(self, mics_coordinates, sound_speed=340, initial_guess=[1, 1], iterations=10, threshold=0.001):
        self.mics_coordinates = mics_coordinates
        self.sound_speed = sound_speed
        self.initial_guess = initial_guess
        self.iterations = iterations
        self.threshold = threshold

    def lat_lon_to_xy(self, longitude, latitude):
        lat_lon = Proj(proj='latlong', datum='WGS84')
        x_y = Proj(proj="utm", zone=10, datum='WGS84')
        x, y = lat_lon(longitude, latitude)
        return x, y

    def equations(self, coordinates, delta_t_measured):
        x, y = coordinates
        eqs = []
        for delta_t, (x_mic, y_mic) in zip(delta_t_measured, self.mics_coordinates):
            eq = np.sqrt((x - x_mic)**2 + (y - y_mic)**2) - self.sound_speed * delta_t
            eqs.append(eq)
        return [eqs[0], eqs[1]]  # Returning values for two equations

    def localize(self, delta_t_measured):
        result = fsolve(self.equations, self.initial_guess, args=(delta_t_measured,))
        return result

# # Example coordinates of microphones
# mics_coordinates = [
#     (-122.4194, 37.7749),  # Example longitude and latitude for microphone 1
#     (-122.4184, 37.7756),  # Example longitude and latitude for microphone 2
#     (-122.4171, 37.7765)   # Example longitude and latitude for microphone 3
# ]
#
# # TDoA measurement values, adding some random noise to simulate uncertainty
# delta_t_measured = [0.001 + np.random.normal(0, 0.0005) for _ in range(len(mics_coordinates))]
#
# # Creating an instance of the localization class
# tdoa_localization = TDoALocalization(mics_coordinates)
#
# # Localizing
# result = tdoa_localization.localize(delta_t_measured)
#
# # Outputting the result
# print("Explosion source coordinates:", result)
