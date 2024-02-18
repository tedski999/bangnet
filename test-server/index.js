const fs = require('fs')
const express = require('express')
const app = express()
const port = 8080

app.post('/', async (req, res) => {
  req.pipe(fs.createWriteStream("image.jpg"));
  res.status(200);
  res.send('ok');
})

app.listen(port, '0.0.0.0', () => {
  console.log(`Example app listening on port ${port}`)
})
