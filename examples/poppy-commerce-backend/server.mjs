import { createServer } from 'node:http';
import { randomUUID } from 'node:crypto';

const port = Number.parseInt(process.env.PORT || '5050', 10);

const products = [
  {
    id: 'prod_keyboard',
    name: 'Poppy Keys Mechanical Keyboard',
    category: 'keyboards',
    price: 12900,
    currency: 'USD',
    inventory: 42,
    options: {
      switch: ['quiet tactile', 'linear', 'clicky'],
      layout: ['ansi', 'iso'],
    },
  },
  {
    id: 'prod_mouse',
    name: 'Poppy Glide Wireless Mouse',
    category: 'accessories',
    price: 6900,
    currency: 'USD',
    inventory: 87,
    options: {
      color: ['graphite', 'sage', 'white'],
    },
  },
  {
    id: 'prod_deskmat',
    name: 'Poppy Desk Mat',
    category: 'accessories',
    price: 2400,
    currency: 'USD',
    inventory: 120,
    options: {
      size: ['compact', 'wide'],
    },
  },
];

const carts = new Map([
  [
    'cart_9001',
    {
      id: 'cart_9001',
      customerId: 'cus_1001',
      currency: 'USD',
      items: [],
      discount: null,
      metadata: { source: 'seed-data' },
      status: 'open',
    },
  ],
]);

const orders = new Map([
  [
    'ord_7001',
    {
      id: 'ord_7001',
      cartId: 'cart_9001',
      customerId: 'cus_1001',
      status: 'processing',
      total: 25800,
      currency: 'USD',
      createdAt: new Date().toISOString(),
    },
  ],
]);

const tickets = new Map();

function sendJson(res, status, body, headers = {}) {
  const payload = JSON.stringify(body, null, 2);
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store',
    ...headers,
  });
  res.end(payload);
}

function sendError(res, status, message, details = {}) {
  sendJson(res, status, {
    error: {
      message,
      ...details,
    },
  });
}

async function readJson(req) {
  let raw = '';

  for await (const chunk of req) {
    raw += chunk;
  }

  if (!raw.trim()) {
    return { raw, json: null };
  }

  try {
    return { raw, json: JSON.parse(raw) };
  } catch {
    const error = new Error('Request body must be valid JSON.');
    error.statusCode = 400;
    throw error;
  }
}

function withRequestEcho(req, url, body = {}) {
  return {
    ...body,
    method: req.method,
    url: `${url.pathname}${url.search}`,
    headers: req.headers,
  };
}

function normalizePath(pathname) {
  return pathname.replace(/^\/anything/, '');
}

function requireAuth(req, res) {
  const authorization = req.headers.authorization || '';
  if (!authorization.startsWith('Bearer ')) {
    sendError(res, 401, 'Missing Bearer token.');
    return false;
  }
  return true;
}

function findCart(id) {
  if (!carts.has(id)) {
    carts.set(id, {
      id,
      customerId: 'cus_1001',
      currency: 'USD',
      items: [],
      discount: null,
      metadata: {},
      status: 'open',
    });
  }

  return carts.get(id);
}

function calculateCart(cart) {
  const subtotal = cart.items.reduce((total, item) => {
    const product = products.find((candidate) => candidate.id === item.productId);
    return total + (product?.price || 0) * item.quantity;
  }, 0);
  const discountAmount = cart.discount ? Math.round(subtotal * 0.2) : 0;

  return {
    subtotal,
    discountAmount,
    total: subtotal - discountAmount,
    currency: cart.currency,
  };
}

async function routeRequest(req, res) {
  const requestUrl = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = normalizePath(requestUrl.pathname);
  const segments = pathname.split('/').filter(Boolean);

  if (req.method === 'GET' && requestUrl.pathname === '/status/200') {
    sendJson(res, 200, {
      status: 'ok',
      service: 'poppy-commerce-backend',
      timestamp: new Date().toISOString(),
    });
    return;
  }

  if (segments[0] !== 'v1') {
    sendError(res, 404, 'Route not found.', { path: requestUrl.pathname });
    return;
  }

  if (req.method === 'POST' && segments.join('/') === 'v1/auth/login') {
    const { json } = await readJson(req);
    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      json,
      accessToken: 'local_live_access_token',
      customer: {
        id: 'cus_1001',
        email: json?.email || 'ada@example.com',
        name: 'Ada Lovelace',
      },
      expiresIn: 3600,
    }));
    return;
  }

  if (req.method === 'GET' && segments[1] === 'stores' && segments[3] === 'products' && segments.length === 4) {
    const category = requestUrl.searchParams.get('category');
    const filtered = category ? products.filter((product) => product.category === category) : products;
    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      storeId: segments[2],
      products: filtered,
      page: Number.parseInt(requestUrl.searchParams.get('page') || '1', 10),
      limit: Number.parseInt(requestUrl.searchParams.get('limit') || String(filtered.length), 10),
      total: filtered.length,
    }));
    return;
  }

  if (req.method === 'GET' && segments[1] === 'stores' && segments[3] === 'products' && segments.length === 5) {
    if (!requireAuth(req, res)) return;
    const product = products.find((candidate) => candidate.id === segments[4]);
    if (!product) {
      sendError(res, 404, 'Product not found.', { productId: segments[4] });
      return;
    }

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      storeId: segments[2],
      product,
    }));
    return;
  }

  if (req.method === 'POST' && segments[1] === 'customers' && segments[3] === 'carts' && segments.length === 4) {
    if (!requireAuth(req, res)) return;
    const { json } = await readJson(req);
    const id = `cart_${randomUUID().slice(0, 8)}`;
    const cart = {
      id,
      customerId: segments[2],
      currency: json?.currency || 'USD',
      items: [],
      discount: null,
      metadata: json?.metadata || {},
      status: 'open',
    };
    carts.set(id, cart);

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      json,
      cart,
      totals: calculateCart(cart),
    }));
    return;
  }

  if (req.method === 'POST' && segments[1] === 'carts' && segments[3] === 'items' && segments.length === 4) {
    if (!requireAuth(req, res)) return;
    const { json } = await readJson(req);
    const cart = findCart(segments[2]);
    const quantity = Number.parseInt(json?.quantity || '1', 10);
    const item = {
      id: `item_${randomUUID().slice(0, 8)}`,
      productId: json?.productId,
      quantity,
      options: json?.options || {},
    };
    cart.items.push(item);

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      json,
      cart,
      item,
      totals: calculateCart(cart),
    }));
    return;
  }

  if (req.method === 'PATCH' && segments[1] === 'carts' && segments[3] === 'discount' && segments.length === 4) {
    if (!requireAuth(req, res)) return;
    const { json } = await readJson(req);
    const cart = findCart(segments[2]);
    cart.discount = {
      code: json?.code || 'POPPY20',
      percentOff: 20,
      appliedAt: new Date().toISOString(),
    };

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      json,
      cart,
      totals: calculateCart(cart),
    }));
    return;
  }

  if (req.method === 'POST' && segments.join('/') === 'v1/checkout/orders') {
    if (!requireAuth(req, res)) return;
    const { json } = await readJson(req);
    const cart = findCart(json?.cartId || 'cart_9001');
    const id = `ord_${randomUUID().slice(0, 8)}`;
    const order = {
      id,
      cartId: json?.cartId,
      customerId: json?.customerId,
      status: 'processing',
      shippingAddress: json?.shippingAddress,
      totals: calculateCart(cart),
      createdAt: new Date().toISOString(),
    };
    orders.set(id, order);

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      json,
      order,
    }));
    return;
  }

  if (req.method === 'GET' && segments[1] === 'orders' && segments.length === 3) {
    if (!requireAuth(req, res)) return;
    const order = orders.get(segments[2]) || {
      id: segments[2],
      cartId: 'cart_9001',
      customerId: 'cus_1001',
      status: 'processing',
      totals: calculateCart(findCart('cart_9001')),
      createdAt: new Date().toISOString(),
    };

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      order,
    }));
    return;
  }

  if (req.method === 'POST' && segments.join('/') === 'v1/support/tickets') {
    if (!requireAuth(req, res)) return;
    const { json } = await readJson(req);
    const ticket = {
      id: `tic_${randomUUID().slice(0, 8)}`,
      status: 'open',
      priority: json?.priority || 'normal',
      subject: json?.subject,
      customerId: json?.customerId,
      orderId: json?.orderId,
      createdAt: new Date().toISOString(),
    };
    tickets.set(ticket.id, ticket);

    sendJson(res, 200, withRequestEcho(req, requestUrl, {
      json,
      ticket,
    }));
    return;
  }

  sendError(res, 404, 'Route not found.', { path: requestUrl.pathname });
}

const server = createServer((req, res) => {
  routeRequest(req, res).catch((error) => {
    const status = error.statusCode || 500;
    sendError(res, status, error.message || 'Unexpected server error.');
  });
});

server.listen(port, () => {
  console.log(`Poppy Commerce backend listening on http://localhost:${port}`);
});

function shutdown(signal) {
  console.log(`Received ${signal}. Shutting down Poppy Commerce backend.`);
  server.close(() => {
    process.exit(0);
  });
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
