// FitPlan API client.
// One small module that every view uses to talk to the REST API.

const TOKEN_KEY = "fitplan.token";
const USER_KEY = "fitplan.user";

// Error type: carries the HTTP status and the problem+json message.
class ApiError extends Error {
  constructor(status, title, detail) {
    super(title);
    this.status = status;
    this.title = title;
    this.detail = detail;
  }
}

// --- session storage -----------------------------------------------
function getToken() {
  return localStorage.getItem(TOKEN_KEY);
}

function setToken(token) {
  localStorage.setItem(TOKEN_KEY, token);
}

function clearToken() {
  localStorage.removeItem(TOKEN_KEY);
}

function getUser() {
  const raw = localStorage.getItem(USER_KEY);
  if (!raw) {
    return null;
  }
  try {
    return JSON.parse(raw);
  } catch (err) {
    return null;
  }
}

function setUser(user) {
  localStorage.setItem(USER_KEY, JSON.stringify(user));
}

function clearUser() {
  localStorage.removeItem(USER_KEY);
}

// A view registers a callback fired whenever the server rejects our token.
let unauthorizedHandler = () => {};

function onUnauthorized(handler) {
  unauthorizedHandler = handler;
}

// --- core request ------------------------------------------------
async function request(method, path, body) {
  const headers = {};

  if (body !== undefined) {
    headers["Content-Type"] = "application/json";
  }

  const token = getToken();
  if (token) {
    headers["Authorization"] = "Bearer " + token;
  }

  const response = await fetch(path, {
    method: method,
    headers: headers,
    body: body !== undefined ? JSON.stringify(body) : undefined,
  });

  // A 401 on a request we sent a token with means the token is no longer
  // valid: wipe the whole local session and bounce to login. A 401 with no
  // token is just a failed login attempt, so let it fall through below.
  if (response.status === 401 && token) {
    clearToken();
    clearUser();
    unauthorizedHandler();
    throw new ApiError(401, "Session expired", "Please log in again.");
  }

  const text = await response.text();
  const data = text ? JSON.parse(text) : null;

  if (!response.ok) {
    const title =
      (data && (data.title || data.detail)) ||
      "Request failed (" + response.status + ")";
    const detail = (data && data.detail) || "";
    throw new ApiError(response.status, title, detail);
  }

  return data;
}

// --- public surface -------------------------------------------
const api = {
  getToken: getToken,
  setToken: setToken,
  clearToken: clearToken,
  getUser: getUser,
  setUser: setUser,
  clearUser: clearUser,
  onUnauthorized: onUnauthorized,
  ApiError: ApiError,
  get: (path) => request("GET", path),
  post: (path, body) => request("POST", path, body),
  put: (path, body) => request("PUT", path, body),
  patch: (path, body) => request("PATCH", path, body),
  del: (path) => request("DELETE", path),
};

window.api = api;