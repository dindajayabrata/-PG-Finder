const API_BASE_URL = '/api';

function getCurrentUser() {
    try {
        const user = JSON.parse(localStorage.getItem('current_user') || 'null');
        if (user && Number.isSafeInteger(user.id) && user.id > 0) return user;
    } catch (err) {
        console.error('Unable to read saved user:', err);
    }
    return null;
}

async function apiRequest(path, options = {}) {
    let response;
    try {
        response = await fetch(`${API_BASE_URL}${path}`, options);
    } catch {
        throw new Error('Cannot connect to the server. Please check that it is running.');
    }
    let data;
    try {
        data = await response.json();
    } catch {
        throw new Error('The server returned an invalid response.');
    }
    if (!response.ok) throw new Error(data.error || 'The request failed.');
    return data;
}

async function getRooms() {
    return await apiRequest('/rooms');
}

async function getFavorites() {
    const user = getCurrentUser();
    if (!user) return [];
    return await apiRequest(`/favorites/${user.id}`);
}

async function toggleFavorite(roomId) {
    const user = getCurrentUser();
    if (!user) {
        alert('Please log in to save favorites.');
        window.location.href = 'login.html';
        return false;
    }
    try {
        await apiRequest('/favorites/toggle', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ userId: user.id, roomId })
        });
        return true;
    } catch (err) {
        alert(err.message);
        return false;
    }
}

function escapeHTML(value) {
    return String(value ?? '')
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;')
        .replaceAll("'", '&#39;');
}

function safeImageUrl(value) {
    try {
        const url = new URL(value);
        if (url.protocol === 'http:' || url.protocol === 'https:') return url.href;
    } catch {
        // Invalid URLs are not included in the image tag.
    }
    return '';
}

function roomCardHTML(room, isFavorite) {
    const imageUrl = safeImageUrl(room.image);
    const imageHTML = imageUrl
        ? `<img src="${escapeHTML(imageUrl)}" alt="${escapeHTML(room.title)}" loading="lazy">`
        : '';
    return `
        <div class="room-card">
            <div class="room-img-wrapper">
                ${imageHTML}
                <button type="button" class="fav-btn ${isFavorite ? 'active' : ''}"
                    data-id="${Number(room.id)}"
                    aria-label="${isFavorite ? 'Remove favorite' : 'Save favorite'}"
                    aria-pressed="${isFavorite}">
                    ${isFavorite ? '❤️' : '🤍'}
                </button>
            </div>
            <div class="room-details">
                <h3>${escapeHTML(room.title)}</h3>
                <p class="location">📍 ${escapeHTML(room.location)}</p>
                <div class="room-meta">
                    <span class="price">₹${Number(room.price).toLocaleString('en-IN')}/mo</span>
                    <span class="badge">${escapeHTML(room.type)}</span>
                </div>
            </div>
        </div>`;
}

function highlightActiveNav() {
    const page = window.location.pathname.split('/').pop() || 'index.html';
    document.querySelectorAll('.nav-links a').forEach(link => {
        link.classList.toggle('active', link.getAttribute('href') === page);
    });
}

document.addEventListener('DOMContentLoaded', () => {
    highlightActiveNav();
    const form = document.getElementById('hero-search-form');
    if (!form) return;
    form.addEventListener('submit', event => {
        event.preventDefault();
        const location = document.getElementById('hero-location').value.trim();
        const type = document.getElementById('hero-room-type').value;
        const params = new URLSearchParams();
        if (location) params.set('location', location);
        if (type) params.set('type', type);
        const query = params.toString();
        window.location.href = query ? `rooms.html?${query}` : 'rooms.html';
    });
});
