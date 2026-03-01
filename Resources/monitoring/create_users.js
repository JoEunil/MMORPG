import http from "http";

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

const API_HOST = "127.0.0.1";
const API_PORT = 3000;
const API_PATH = "/auth/signup";

async function main() {
    for (let i = 1; i <= 5000; i++) {
        const data = JSON.stringify({
            username: `test${i}`,
            password: "12345",
            email: `test${i}@test.com`,
            phone_number: `010-0000-${String(i).padStart(4, "0")}`
        });

        const options = {
            hostname: API_HOST,
            port: API_PORT,
            path: API_PATH,
            method: "POST",
            headers: {
                "Content-Type": "application/json",
                "Content-Length": data.length
            }
        };

        const req = http.request(options, res => {
            console.log(`${i} → Status: ${res.statusCode}`);
        });

        req.on("error", e => console.error(`Error: ${i} → ${e}`));

        req.write(data);
        req.end();

        await sleep(100); // 100ms 대기 (초당 약 10개)
    }
}

main();
