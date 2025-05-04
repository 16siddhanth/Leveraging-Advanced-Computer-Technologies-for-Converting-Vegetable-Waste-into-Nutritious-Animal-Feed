import os
import secrets
from flask import Flask, render_template, request, redirect, url_for, session, flash
from flask_sqlalchemy import SQLAlchemy
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(__name__)

# App configuration
app.secret_key = os.getenv('FLASK_SECRET_KEY', secrets.token_hex(16))  # Secure secret key
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///waste_data.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

# Initialize database
db = SQLAlchemy(app)

# Database models
class User(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(50), unique=True, nullable=False)
    password = db.Column(db.String(255), nullable=False)

class WasteData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, nullable=False)
    organic_weight = db.Column(db.Float, nullable=False)
    nonorganic_weight = db.Column(db.Float, nullable=False)
    temperature = db.Column(db.Float, nullable=False)
    timestamp = db.Column(db.DateTime, default=db.func.current_timestamp())

# Routes
@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']

        # Check user in the database
        user = User.query.filter_by(username=username).first()
        if user and check_password_hash(user.password, password):  # Verify hashed password
            session['user_id'] = user.id
            session['username'] = user.username
            flash('Login successful!', 'success')
            return redirect(url_for('home'))
        else:
            flash('Invalid username or password.', 'error')

    return render_template('login.html')

@app.route('/signup', methods=['GET', 'POST'])
def signup():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']

        if User.query.filter_by(username=username).first():
            flash('Username already exists. Please choose a different one.', 'error')
        else:
            hashed_password = generate_password_hash(password)  # Hash the password
            new_user = User(username=username, password=hashed_password)
            db.session.add(new_user)
            db.session.commit()
            flash('Account created successfully! Please log in.', 'success')
            return redirect(url_for('login'))

    return render_template('signup.html')

@app.route('/')
def home():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    return render_template('home.html')

@app.route('/userpage', methods=['GET', 'POST'])
def userpage():
    if 'user_id' not in session:
        return redirect(url_for('login'))

    if request.method == 'POST':
        try:
            organic_weight = float(request.form['organic_weight'])
            nonorganic_weight = float(request.form['nonorganic_weight'])
            temperature = float(request.form['temperature'])
        except ValueError:
            flash('Please enter valid numbers.', 'error')
            return render_template('userpage.html')

        waste_data = WasteData(
            user_id=session['user_id'],
            organic_weight=organic_weight,
            nonorganic_weight=nonorganic_weight,
            temperature=temperature
        )
        db.session.add(waste_data)
        db.session.commit()
        flash('Data saved successfully.', 'success')

    return render_template('userpage.html')

@app.route('/userdata')
def userdata():
    if 'user_id' not in session:
        return redirect(url_for('login'))

    user_id = session['user_id']
    waste_entries = WasteData.query.filter_by(user_id=user_id).all()
    return render_template('userdata.html', waste_entries=waste_entries)

@app.route('/logout')
def logout():
    session.pop('user_id', None)
    session.pop('username', None)
    flash('Logged out successfully.', 'success')
    return redirect(url_for('login'))

@app.route('/about')
def about():
    return render_template('about.html')

@app.route('/service')
def service():
    return render_template('service.html')

@app.errorhandler(404)
def page_not_found(e):
    return render_template('error.html', error=e), 404

# Main entry point
if __name__ == '__main__':
    with app.app_context():
        db.create_all()  # Ensure tables are created
    app.run(debug=True)
