import numpy as np
from scipy.optimize import least_squares
import requests

class TDoALocalization:
    def __init__(self, mics_coordinates, temperature, humidity, initial_guess=[0, 0]):
        # Initialize with microphone coordinates, ambient temperature and humidity, and an initial guess for the source position
        self.mics_coordinates = mics_coordinates
        self.temperature = temperature
        self.humidity = humidity
        self.sound_speed = self.calculate_sound_speed(temperature, humidity)  # Calculate initial sound speed
        self.initial_guess = initial_guess

    def calculate_sound_speed(self, temperature, humidity):
        # Convert temperature from Kelvin to Celsius and calculate sound speed
        temperature_celsius = temperature - 273.15
        return 331.3 * np.sqrt(1 + temperature_celsius / 273.15) + 0.6 * humidity

    def equations(self, coordinates, delta_t_measured):
        # Calculate residuals for each microphone based on the difference between predicted and actual distances
        x, y = coordinates
        residuals = []
        for delta_t, (x_mic, y_mic) in zip(delta_t_measured, self.mics_coordinates):
            predicted_distance = np.sqrt((x - x_mic)**2 + (y - y_mic)**2)
            time_distance = self.sound_speed * delta_t
            residual = predicted_distance - time_distance
            residuals.append(residual)
        return residuals

    def localize(self, delta_t_measured):
        # Use least_squares method to find the source position that minimizes the sum of squared residuals
        result = least_squares(self.equations, self.initial_guess, args=(delta_t_measured,))
        return result.x  # The optimized source position

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

def simulate_explosions(num_simulations, num_sensors, camera_coords, area_size=100):
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
camera_coords = [50, 50]  # Example camera coordinates
average_error, error_std, average_camera_angle = simulate_explosions(1000, 4, camera_coords)

# Output the results of the simulation
print(f"Average error: {average_error}")
print(f"Error standard deviation: {error_std}")
print(f"Average camera angle from North: {average_camera_angle} degrees")
