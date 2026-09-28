# Poppy Commerce Backend

This is a dependency-free local backend for the `examples/poppy-commerce-demo` collection.

## Run

```powershell
npm start --prefix examples/poppy-commerce-backend
```

Or run it directly without `npm`:

```powershell
node examples/poppy-commerce-backend/server.mjs
```

The server listens on `http://localhost:5050` by default. To use another port:

```powershell
$env:PORT = "6060"
node examples/poppy-commerce-backend/server.mjs
```

If you change the port, update `examples/poppy-commerce-demo/environments/local-live.env`.

## Use With Poppy

1. Import `examples/poppy-commerce-demo` as a collection.
2. Start this backend.
3. Select the `local-live` environment.
4. Run the requests in order from `auth`, `catalog`, `cart`, and `checkout`.

The backend supports the same `/anything/v1/...` route shape as the httpbin-based `dev` environment, so the collection can switch between echoed demo responses and live local data by changing environments.
