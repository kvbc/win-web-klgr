const ProtectedTextAPI = require('protectedtext-api');
const fs = require('fs');

(async function() {
    const [id, pwd] = fs.readFileSync('protected-text-idpwd.txt', 'utf8').split(',');
    await (await new ProtectedTextAPI(id, pwd).loadTabs()).save(fs.readFileSync('klgr.log', 'utf8'));
})();