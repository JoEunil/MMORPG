const redis = require("redis");
require("dotenv").config();

const redisClient = redis.createClient({
    host: process.env.REDIS_HOST || 'localhost',
    port: process.env.REDIS_PORT || 6379,
    password: process.env.REDIS_PASSWORD || null, 
});

redisClient.connect();

redisClient.on("connect", () => {
    console.log("Redis connected!");
});

redisClient.on("error", (err) => {
    console.error("Redis connection error:", err);
});
// 다른 파일에서 사용 가능하도록 export
module.exports = redisClient;