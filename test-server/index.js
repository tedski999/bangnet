const fs = require('fs')
const express = require('express')
const app = express()
const port = 8080

app.post('/', async (req, res) => {
  req.pipe(fs.createWriteStream("image.jpg"));
  res.status(200);
  res.send('ok');
})

function getRandomInt(min, max) {
  min = Math.ceil(min);
  max = Math.floor(max);
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

app.get('/servo', (req, res) => {
  let minServoAngle = 0;
  let maxServoAngle = 180;
  let servoAngle = getRandomInt(minServoAngle, maxServoAngle);
  
  res.status(200);
  res.send(servoAngle.toString());
})

app.listen(port, '0.0.0.0', () => {
  console.log(`Example app listening on port ${port}`)
})
