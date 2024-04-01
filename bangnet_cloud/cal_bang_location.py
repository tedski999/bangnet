import numpy as np
from scipy.optimize import least_squares
import requests


class TDoALocalization:
    def __init__(self, mics_coordinates, temperature, humidity, initial_guess=[0, 0]):
        self.mics_coordinates = np.array(mics_coordinates)  # Convert microphone coordinates to NumPy array
        self.temperature = temperature  # Store temperature
        self.humidity = humidity  # Store humidity
        self.sound_speed = self.calculate_sound_speed(temperature, humidity)  # Calculate sound speed
        self.initial_guess = initial_guess  # Store initial guess for source position

    def calculate_sound_speed(self, temperature, humidity):
        # Convert temperature from Kelvin to Celsius
        temperature_celsius = temperature - 273.15
        # Update the formula for calculating sound speed
        return 331.3 + (0.606 * humidity) + (0.0124 * temperature_celsius)

    def equations(self, source_position, delta_t_measured):
        lon, lat = source_position  # Extract longitude and latitude of the source position
        residuals = []  # List to store residuals
        for i, (lon_mic, lat_mic) in enumerate(self.mics_coordinates):
            distance = haversine(lon_mic, lat_mic, lon, lat)  # Calculate distance between microphone and source
            predicted_time = distance / self.sound_speed  # Predicted time for sound wave to travel
            if i > 0:
                measured_time_diff = delta_t_measured[i - 1]  # Measured time difference
                # Predicted time difference from reference microphone to current microphone
                predicted_time_diff = predicted_time - (haversine(self.mics_coordinates[0][1], self.mics_coordinates[0][0], lon, lat) / self.sound_speed)
                residuals.append(predicted_time_diff - measured_time_diff)  # Calculate and store residual
        return residuals  # Return list of residuals

    def localize(self, timestamps):
        result = least_squares(self.equations, self.initial_guess, args=(timestamps,))  # Perform least squares optimization
        return result.x  # Return optimized source position


def haversine(lon1, lat1, lon2, lat2):
    # Convert coordinates to radians
    lon1, lat1, lon2, lat2 = map(np.radians, [lon1, lat1, lon2, lat2])

    # Haversine formula
    dlon = lon2 - lon1
    dlat = lat2 - lat1
    a = np.sin(dlat/2)**2 + np.cos(lat1) * np.cos(lat2) * np.sin(dlon/2)**2
    c = 2 * np.arcsin(np.sqrt(a))
    r = 6371000  # Earth radius in meters
    return c * r  # Return haversine distance


def get_environment_conditions():
    # Fetch current environmental conditions from an external API using an API key stored in an environment variable
    API_KEY = '2a7a2b50f5f913196eba18d387c2f54d'
    lat, lon = 37.7021, -121.9358  # Dublin's latitude and longitude
    url = f'https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={API_KEY}'

    try:
        response = requests.get(url)
        response.raise_for_status()  # Check for HTTP request errors
        data = response.json()
        temperature = data['main']['temp']  # Temperature in Kelvin
        humidity = data['main']['humidity']  # Humidity in percentage
        return temperature, humidity
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")
        return 273.15 + 20, 50  # Default values if request fails
    
def calculate_bearing(lat1, lon1, lat2, lon2):
    # Convert latitude and longitude from degrees to radians
    lat1, lon1, lat2, lon2 = map(np.radians, [lat1, lon1, lat2, lon2])

    dlon = lon2 - lon1
    x = np.sin(dlon) * np.cos(lat2)
    y = np.cos(lat1) * np.sin(lat2) - np.sin(lat1) * np.cos(lat2) * np.cos(dlon)

    initial_bearing = np.arctan2(x, y)
    initial_bearing = np.degrees(initial_bearing)
    compass_bearing = (initial_bearing + 360) % 360

    return compass_bearing

def simulate_explosions(num_simulations, num_sensors, area_size=100):
    # Simulate explosion events and localize the source based on sensor data
    estimated_positions = []
    true_positions = []
    camera_angles = []
    # Don't move this code into for loop, It will cost a lot.
    temperature, humidity = get_environment_conditions()  # Fetch environmental conditions for each simulation


    for _ in range(num_simulations):
        explosion_source = np.random.rand(2) * area_size
        sensors = np.random.rand(num_sensors, 2) * area_size

        localization = TDoALocalization(sensors, temperature, humidity)
        distances = np.sqrt(np.sum((sensors - explosion_source) ** 2, axis=1))
        times = distances / localization.sound_speed

        noise = np.random.normal(0, 0.01, times.shape)
        times_noisy = times + noise
        delta_t_measured = times_noisy - times_noisy[0]

        estimated_position = localization.localize(delta_t_measured)
        true_positions.append(explosion_source)
        estimated_positions.append(estimated_position)

        # Generate random camera coordinates
        camera_coords = np.random.rand(2) * area_size

        # Calculate the camera angle for each simulation
        camera_angle = calculate_bearing(camera_coords[0], camera_coords[1], estimated_position[0], estimated_position[1])
        camera_angles.append(camera_angle)

    true_positions = np.array(true_positions)
    estimated_positions = np.array(estimated_positions)
    errors = np.sqrt(np.sum((true_positions - estimated_positions) ** 2, axis=1))

    # Example output of camera angles for the last simulation
    print(f"Camera needs to rotate to {camera_angles[-1]:.2f} degrees relative to North for the last simulation.")

    return np.mean(errors), np.std(errors), np.mean(camera_angles)


# Run the simulation with specified parameters
def simulation():
    average_error, error_std, average_camera_angle = simulate_explosions(1000, 4)

    # Output the results of the simulation
    print(f"Average error: {average_error}")
    print(f"Error standard deviation: {error_std}")

# usage
def usage():
    mics_coordinates = [(lat1, lon1), (lat2, lon2), ...]  
    temperature, humidity = get_environment_conditions()
    #initial_guess = [initial_x, initial_y]
    delta_t_measured = [delta_t1, delta_t2, ...]  
    localizer = TDoALocalization(mics_coordinates, temperature, humidity)
    estimated_source_position = localizer.localize(delta_t_measured)
    print("Estimated source position:", estimated_source_position)
    camera_lat = ...
    camera_lon = ...
    explosion_bearing = calculate_bearing(camera_lat, camera_lon, estimated_source_position[0], estimated_source_position[1])
    print("Explosion bearing:", explosion_bearing)


if __name__ == "__main__":
    #simulation()
    usage()
