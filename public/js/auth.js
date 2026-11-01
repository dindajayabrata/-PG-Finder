document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('login-form');
    const registerForm = document.getElementById('register-form');
    if (loginForm) {
        loginForm.addEventListener('submit', async event => {
            event.preventDefault();
            const email = document.getElementById('login-email').value.trim();
            const password = document.getElementById('login-password').value;
            const button = loginForm.querySelector('button[type="submit"]');
            button.disabled = true;
            try {
                const data = await apiRequest('/login', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ email, password })
                });
                localStorage.setItem('current_user', JSON.stringify(data.user));
                localStorage.removeItem('pg_favorites');
                alert('Login successful!');
                window.location.href = 'index.html';
            } catch (err) {
                alert(err.message);
            } finally {
                button.disabled = false;
            }
        });
    }
    if (registerForm) {
        registerForm.addEventListener('submit', async event => {
            event.preventDefault();
            const newUser = {
                name: document.getElementById('reg-name').value.trim(),
                email: document.getElementById('reg-email').value.trim(),
                password: document.getElementById('reg-password').value
            };
            if (!newUser.name || !newUser.password.trim()) {
                alert('Enter your name and a valid password.');
                return;
            }
            const button = registerForm.querySelector('button[type="submit"]');
            button.disabled = true;
            try {
                await apiRequest('/register', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(newUser)
                });
                alert('Account registered successfully! Please log in.');
                window.location.href = 'login.html';
            } catch (err) {
                alert(err.message);
            } finally {
                button.disabled = false;
            }
        });
    }
});
