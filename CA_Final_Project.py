"""
Control and Automation for the Efficient Use of Energy
Final Project

@authors: Sara Kiprijanova, Alice Scalamandrè, Laura Amaro
"""
# Importing libraries 
import serial 
import requests 
import json
import time
from datetime import datetime, timedelta
import numpy as np
import pandas as pd

# API start date definition
start_date = datetime.now()
start_hour = datetime.now().hour 

# Hours untill midnight
max_hours_ahead = 24 - start_hour

# API end date definition to see the prices ahead
end_date = start_date + timedelta(hours=max_hours_ahead) 
            
# Transform into strings with a minute resolution
end_date = end_date.strftime('%Y-%m-%dT%H:%M')
start_date = start_date.strftime('%Y-%m-%dT%H:%M')

# Get data from API
endpoint = 'https://apidatos.ree.es' 
get_archives = '/en/datos/mercados/precios-mercados-tiempo-real'
headers = {'Accept': 'application/json',
           'Content-Type': 'application/json',
           'Host': 'apidatos.ree.es'}
params = {'start_date': start_date, 'end_date': end_date, 'time_trunc':'hour'}
response = requests.get(endpoint+get_archives, headers=headers, params=params)
data_json = response.json()

# Data forecasted
spot_price_values = data_json["included"][1]["attributes"]["values"]
spot_price = []

# Create a vector just with price values
for time_period in spot_price_values:
    spot_price.append(time_period['value'])

# Creation of the hours vector    
hours_vect = list(range(max_hours_ahead)) + np.ones(max_hours_ahead) * int(start_hour)

# Creating a DataFrame with the needed values
# The default status is 'OFF' and will be changed later
data = {'Hour':hours_vect, 'Price':spot_price, 'Status':['OFF'] * max_hours_ahead}
df=pd.DataFrame(data)

# Helping variable to enter specific rows of the df
loop_number = 0

# Creating communications object with Arduino using Serial 
arduino = serial.Serial('COM6', 9600)
print("Communication established.")

try:
    while True:
        # Specify for how much time the EV needs to be charging
        charging_time = 6
        
        now = datetime.time
        # READ DATA    
        # Check if there is new info from the Arduino and read it
        data_bytes = arduino.readline()
        
        # Decoding the message into UTF-8         
        data_ide = data_bytes.decode("utf-8")
        # Retrieving the value of hours by the User in Telegram 
        user_value = int(data_ide)
        # Check if those amount of hours go beyond midnight
        if user_value > max_hours_ahead:
            user_value = max_hours_ahead
        if user_value < charging_time:
            charging_time = user_value
            
        # Take just the next x hours
        df1 = df.head(user_value)        

        # Parsing the dataframe
        for index, row in df1.iterrows() :
            # Looking for the lowest prices (the same amount of charging time)
            if index in df1.nsmallest(charging_time, ['Price']).index.values:
                # Changing the Status from 'OFF' to 'ON'
                df1.at[index,'Status']='ON'

        # Print the df to check the charging schedule
        print(df1) 
        
        # Reset the timer for the new loop
        previous_time = datetime.now()
        starttime = time.time()
        
        for index, row in df1.iterrows():
            actual_time = datetime.now()
            # Communication between the Computer and Relay to enable car charging
            if row['Status']=='ON':
                arduino.write('H'.encode())
                print ('Your car is charging.')
                data_bytes = arduino.readline()
                
                # Decoding the message into UTF-8         
                data_ide = data_bytes.decode("utf-8")
                user_value = int(data_ide)
                print(user_value)
            
            else:
                arduino.write('L'.encode())
                print ('Waiting for lower electricity price.')
                
            # Waiting time before entering another loop, equivalent to 1h of real time
            time.sleep(30.0) 
              
# Handling KeyboardInterrupt by the end-user (CTRL+C)
except KeyboardInterrupt:
# Closing communications port
    arduino.close() 
    print('Communications closed.')