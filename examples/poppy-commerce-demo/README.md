# Poppy Commerce Demo API

This is an importable mock collection for Poppy. It models a small commerce API with auth, catalog, cart, checkout, support, and admin requests.

Use the `dev` environment to run against `https://httpbin.org` and see echoed request data. Use the `local-mock` environment when testing Poppy's embedded mock server on `http://localhost:8080`.

## Suggested Demo Flow

1. Import this folder as a collection in Poppy.
2. Select the `dev` environment.
3. Run `Login Customer`, then `List Products`, then `Create Cart`, then `Create Order`.
4. Try `Tools -> Mock Server...` and import routes from the active collection.
5. Switch to `local-mock` and exercise the same requests against your local mock server.
